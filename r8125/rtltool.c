// SPDX-License-Identifier: GPL-2.0-only
/*
################################################################################
#
# r8125 is the Linux device driver released for Realtek 2.5Gigabit Ethernet
# controllers with PCI-Express interface.
#
# Copyright(c) 2021 Realtek Semiconductor Corp. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#
# Author:
# Realtek NIC software team <nicfae@realtek.com>
# No. 2, Innovation Road II, Hsinchu Science Park, Hsinchu 300, Taiwan
#
################################################################################
*/

/************************************************************************************
 *  This product is covered by one or more of the following patents:
 *  US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

#include <linux/module.h>
//The vmkernel network api is from 2.6.24,not the default 2.6.18
#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#undef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(2,6,24)
#endif 
//The vmkernel network api is from 2.6.24,not the default 2.6.18
#ifdef LINUX_VERSION_CODE
#undef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(2,6,24)
#endif 
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/in.h>
#include <linux/ethtool.h>
#include <asm/uaccess.h>
#include "r8125.h"
#include "rtl_eeprom.h"
#include "rtltool.h"

int rtl8125_tool_ioctl(struct rtl8125_private *tp, struct ifreq *ifr)
{
        struct rtltool_cmd my_cmd;
        int ret;

        if (copy_from_user(&my_cmd, ifr->ifr_data, sizeof(my_cmd)))
                return -EFAULT;

        ret = 0;
        switch (my_cmd.cmd) {
        case RTLTOOL_READ_MAC:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                if (my_cmd.len==1)
                        my_cmd.data = readb(tp->mmio_addr+my_cmd.offset);
                else if (my_cmd.len==2)
                        my_cmd.data = readw(tp->mmio_addr+(my_cmd.offset&~1));
                else if (my_cmd.len==4)
                        my_cmd.data = readl(tp->mmio_addr+(my_cmd.offset&~3));
                else {
                        ret = -EOPNOTSUPP;
                        break;
                }

                if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
                        ret = -EFAULT;
                        break;
                }
                break;

        case RTLTOOL_WRITE_MAC:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                if (my_cmd.len==1)
                        writeb(my_cmd.data, tp->mmio_addr+my_cmd.offset);
                else if (my_cmd.len==2)
                        writew(my_cmd.data, tp->mmio_addr+(my_cmd.offset&~1));
                else if (my_cmd.len==4)
                        writel(my_cmd.data, tp->mmio_addr+(my_cmd.offset&~3));
                else {
                        ret = -EOPNOTSUPP;
                        break;
                }

                break;

        case RTLTOOL_READ_PHY:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                my_cmd.data = rtl8125_mdio_prot_read(tp, my_cmd.offset);
                if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
                        ret = -EFAULT;
                        break;
                }

                break;

        case RTLTOOL_WRITE_PHY:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                rtl8125_mdio_prot_write(tp, my_cmd.offset, my_cmd.data);
                break;

        case RTLTOOL_READ_EPHY:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                my_cmd.data = rtl8125_ephy_read(tp, my_cmd.offset);
                if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
                        ret = -EFAULT;
                        break;
                }

                break;

        case RTLTOOL_WRITE_EPHY:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                rtl8125_ephy_write(tp, my_cmd.offset, my_cmd.data);
                break;

        case RTLTOOL_READ_ERI:
                my_cmd.data = 0;
                if (my_cmd.len==1 || my_cmd.len==2 || my_cmd.len==4) {
                        my_cmd.data = rtl8125_eri_read(tp, my_cmd.offset, my_cmd.len, ERIAR_ExGMAC);
                } else {
                        ret = -EOPNOTSUPP;
                        break;
                }

                if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
                        ret = -EFAULT;
                        break;
                }

                break;

        case RTLTOOL_WRITE_ERI:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                if (my_cmd.len==1 || my_cmd.len==2 || my_cmd.len==4) {
                        rtl8125_eri_write(tp, my_cmd.offset, my_cmd.len, my_cmd.data, ERIAR_ExGMAC);
                } else {
                        ret = -EOPNOTSUPP;
                        break;
                }
                break;

        case RTLTOOL_READ_PCI:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                my_cmd.data = 0;
                if (my_cmd.len==1)
                        pci_read_config_byte(tp->pci_dev, my_cmd.offset,
                                             (u8 *)&my_cmd.data);
                else if (my_cmd.len==2)
                        pci_read_config_word(tp->pci_dev, my_cmd.offset,
                                             (u16 *)&my_cmd.data);
                else if (my_cmd.len==4)
                        pci_read_config_dword(tp->pci_dev, my_cmd.offset,
                                              &my_cmd.data);
                else {
                        ret = -EOPNOTSUPP;
                        break;
                }

                if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
                        ret = -EFAULT;
                        break;
                }
                break;

        case RTLTOOL_WRITE_PCI:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                if (my_cmd.len==1)
                        pci_write_config_byte(tp->pci_dev, my_cmd.offset,
                                              my_cmd.data);
                else if (my_cmd.len==2)
                        pci_write_config_word(tp->pci_dev, my_cmd.offset,
                                              my_cmd.data);
                else if (my_cmd.len==4)
                        pci_write_config_dword(tp->pci_dev, my_cmd.offset,
                                               my_cmd.data);
                else {
                        ret = -EOPNOTSUPP;
                        break;
                }

                break;

        case RTLTOOL_READ_EEPROM:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                my_cmd.data = rtl8125_eeprom_read_sc(tp, my_cmd.offset);
                if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
                        ret = -EFAULT;
                        break;
                }

                break;

        case RTLTOOL_WRITE_EEPROM:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                rtl8125_eeprom_write_sc(tp, my_cmd.offset, my_cmd.data);
                break;

        case RTL_READ_OOB_MAC:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                rtl8125_oob_mutex_lock(tp);
                my_cmd.data = rtl8125_ocp_read(tp, my_cmd.offset, 4);
                rtl8125_oob_mutex_unlock(tp);
                if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
                        ret = -EFAULT;
                        break;
                }
                break;

        case RTL_WRITE_OOB_MAC:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                if (my_cmd.len == 0 || my_cmd.len > 4)
                        return -EOPNOTSUPP;

                rtl8125_oob_mutex_lock(tp);
                rtl8125_ocp_write(tp, my_cmd.offset, my_cmd.len, my_cmd.data);
                rtl8125_oob_mutex_unlock(tp);
                break;

        case RTL_ENABLE_PCI_DIAG:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                tp->rtk_enable_diag = 1;

                dprintk("enable rtk diag\n");
                break;

        case RTL_DISABLE_PCI_DIAG:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                tp->rtk_enable_diag = 0;

                dprintk("disable rtk diag\n");
                break;

        case RTL_READ_MAC_OCP:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                if (my_cmd.offset % 2)
                        return -EOPNOTSUPP;

                my_cmd.data = rtl8125_mac_ocp_read(tp, my_cmd.offset);
                if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
                        ret = -EFAULT;
                        break;
                }
                break;

        case RTL_WRITE_MAC_OCP:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                if ((my_cmd.offset % 2) || (my_cmd.len != 2))
                        return -EOPNOTSUPP;

                rtl8125_mac_ocp_write(tp, my_cmd.offset, (u16)my_cmd.data);
                break;

        case RTL_DIRECT_READ_PHY_OCP:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                my_cmd.data = rtl8125_mdio_prot_direct_read_phy_ocp(tp, my_cmd.offset);
                if (copy_to_user(ifr->ifr_data, &my_cmd, sizeof(my_cmd))) {
                        ret = -EFAULT;
                        break;
                }

                break;

        case RTL_DIRECT_WRITE_PHY_OCP:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;

                rtl8125_mdio_prot_direct_write_phy_ocp(tp, my_cmd.offset, my_cmd.data);
                break;

        default:
                ret = -EOPNOTSUPP;
                break;
        }

        return ret;
}
