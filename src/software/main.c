#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <fpga_pci.h>
#include <fpga_mgmt.h>
#include <utils/lcd.h>

#define IN_BUF_SIZE 8
#define OUT_BUF_SIZE 32

#define DDR_A_BASE UINT64_C(0x0000000000)
#define DDR_B_BASE UINT64_C(0x0080000000)
#define DDR_D_BASE UINT64_C(0x0100000000)


static uint16_t pci_vendor_id = 0x1D0F; /* Amazon PCI Vendor ID */
static uint16_t pci_device_id = 0xF000; /* PCI Device ID preassigned by Amazon for F1 applications */

int check_afi_ready(int slot_id);

int main(int argc, char *argv[])
{
    int rc;

    rc = fpga_pci_init();
    fail_on(rc, out, "Unable to initialize the fpga_pci library");

    rc = check_afi_ready(0);
    fail_on(rc, out, "AFI not ready");

    int a[8],b[8];

    // Read inputs for A
    for (int i = 0; i < 8; i++) {
        printf("%2d> ", i+1);
        scanf("%d", &a[i]);
    }

    // Read inputs for B
    for (int i = 0; i < 8; i++) {
        printf("%2d> ", i+1);
        scanf("%d", &b[i]);
    }

    printf("Expected result for: ");
    int expected = 0;
    for (int i = 0; i < 8; i++) {
        printf("%d * %d", (int) a[i], (int) b[i]);
        if (i < 7) {
            printf(" + ");
        }

        expected += (int) a[i] * (int) b[i];
    }
    printf(" = %d\n", expected);


    // Attach PCIe
    pci_bar_handle_t pci_bar_handle = PCI_BAR_HANDLE_INIT;
    rc = fpga_pci_attach(0, FPGA_APP_PF, APP_PF_BAR4, BURST_CAPABLE, &pci_bar_handle);
    fail_on(rc, out, "Unable to attach to the AFI on slot id %d", 0);

    // Write in burst mode A and B into DDR_A, DDR_B and DDR_D

    // DDR_A
    rc = fpga_pci_write_burst(pci_bar_handle, DDR_A_BASE, (uint32_t*) a, 2);
    rc = fpga_pci_write_burst(pci_bar_handle, DDR_A_BASE + 256, (uint32_t*) b, 2);

    fail_on(rc, out, "Write failed!");

    // Read from each DDR
    uint32_t valueA;
    uint32_t valueB;
    for (int i = 0; i < 8; i++) {
        rc = fpga_pci_peek(pci_bar_handle, DDR_A_BASE + i * 32, &valueA);
        rc = fpga_pci_peek(pci_bar_handle, DDR_A_BASE + 256 + i * 32, &valueA);
        fail_on(rc, out, "Read failed!");

        printf("A Value #%d in DDR A: %d\n", i, (int) valueA);
        printf("B Value #%d in DDR A: %d\n", i, (int) valueB);
    }

out:
    if (pci_bar_handle >= 0) {
        rc = fpga_pci_detach(pci_bar_handle);
        if (rc) {
            printf("Failure while detaching from FPGA.\n");
        }
    }

    return (rc != 0 ? 1 : 0);
}

int check_afi_ready(int slot_id)
{
    struct fpga_mgmt_image_info info = {0};
    int rc;

    /* get local image description, contains status, vendor id, and device id. */
    rc = fpga_mgmt_describe_local_image(slot_id, &info,0);
    fail_on(rc, out, "Unable to get AFI information from slot %d. Are you running as root?",slot_id);

    /* check to see if the slot is ready */
    if (info.status != FPGA_STATUS_LOADED) {
        rc = 1;
        fail_on(rc, out, "AFI in Slot %d is not in READY state !", slot_id);
    }

    printf("AFI PCI  Vendor ID: 0x%x, Device ID 0x%x\n",
        info.spec.map[FPGA_APP_PF].vendor_id,
        info.spec.map[FPGA_APP_PF].device_id);

    /* confirm that the AFI that we expect is in fact loaded */
    if (info.spec.map[FPGA_APP_PF].vendor_id != pci_vendor_id ||
        info.spec.map[FPGA_APP_PF].device_id != pci_device_id) {
        printf("AFI does not show expected PCI vendor id and device ID. If the AFI "
               "was just loaded, it might need a rescan. Rescanning now.\n");

        rc = fpga_pci_rescan_slot_app_pfs(slot_id);
        fail_on(rc, out, "Unable to update PF for slot %d",slot_id);
        /* get local image description, contains status, vendor id, and device id. */
        rc = fpga_mgmt_describe_local_image(slot_id, &info,0);
        fail_on(rc, out, "Unable to get AFI information from slot %d",slot_id);

        printf("AFI PCI  Vendor ID: 0x%x, Device ID 0x%x\n",
            info.spec.map[FPGA_APP_PF].vendor_id,
            info.spec.map[FPGA_APP_PF].device_id);

        /* confirm that the AFI that we expect is in fact loaded after rescan */
        if (info.spec.map[FPGA_APP_PF].vendor_id != pci_vendor_id ||
             info.spec.map[FPGA_APP_PF].device_id != pci_device_id) {
            rc = 1;
            fail_on(rc, out, "The PCI vendor id and device of the loaded AFI are not "
                             "the expected values.");
        }
    }

    return rc;

out:
    return 1;
}
