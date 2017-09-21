#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <fpga_pci.h>
#include <fpga_mgmt.h>
#include <utils/lcd.h>

#define IN_BUF_SIZE 8
#define OUT_BUF_SIZE 32

// TODO: This should be defined in a separate .h file
// Memory address offsets
#define INPUT_A0_OFFSET UINT64_C(0x0040000000)
#define INPUT_A1_OFFSET UINT64_C(0x0040010000)
#define INPUT_A2_OFFSET UINT64_C(0x0040020000)
#define INPUT_A3_OFFSET UINT64_C(0x0040030000)
#define INPUT_A4_OFFSET UINT64_C(0x0040040000)
#define INPUT_A5_OFFSET UINT64_C(0x0040050000)
#define INPUT_A6_OFFSET UINT64_C(0x0040060000)
#define INPUT_A7_OFFSET UINT64_C(0x0040070000)

#define INPUT_B0_OFFSET UINT64_C(0x0040080000)
#define INPUT_B1_OFFSET UINT64_C(0x0040090000)
#define INPUT_B2_OFFSET UINT64_C(0x00400A0000)
#define INPUT_B3_OFFSET UINT64_C(0x00400B0000)
#define INPUT_B4_OFFSET UINT64_C(0x00400C0000)
#define INPUT_B5_OFFSET UINT64_C(0x00400D0000)
#define INPUT_B6_OFFSET UINT64_C(0x00400E0000)
#define INPUT_B7_OFFSET UINT64_C(0x00400F0000)

#define OUTPUT_OFFSET UINT64_C(0x0040100000)

static uint16_t pci_vendor_id = 0x1D0F; /* Amazon PCI Vendor ID */
static uint16_t pci_device_id = 0xF000; /* PCI Device ID preassigned by Amazon for F1 applications */

int check_afi_ready(int slot_id);
pci_bar_handle_t attach_fpga();
int fma_8(int8_t a[8], int8_t b[8], uint32_t *result, pci_bar_handle_t pci_bar_handle);

int main(int argc, char *argv[])
{
    int rc;
    pci_bar_handle_t pci_bar_handle;

    rc = fpga_pci_init();
    fail_on(rc, out, "Unable to initialize the fpga_pci library");

    rc = check_afi_ready(0);
    fail_on(rc, out, "AFI not ready");

    // Attach FPGA
    pci_bar_handle = attach_fpga();

    if (pci_bar_handle < 0) {
        return 1;
    }

    uint32_t result;

    int8_t a[8],b[8];
    int value;

    // Read inputs for A
    for (int i = 0; i < 8; i++) {
        printf("%2d> ", i+1);
        scanf("%d", &value);
        a[i] = value;
    }

    // Read inputs for B
    for (int i = 0; i < 8; i++) {
        printf("%2d> ", i+1);
        scanf("%d", &value);
        b[i] = value;
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

    rc = fma_8(a, b, &result, pci_bar_handle);
    fail_on(rc, out, "Fused multiply add failed");
    printf("Actual: %d\n", (int) result);

    return rc;

out:
    return 1;
}

int fma_8(
    int8_t a[8],
    int8_t b[8],
    uint32_t *result,
    pci_bar_handle_t pci_bar_handle
)
{
    int rc;

    // Setup addresses
    uint64_t input_a_addr[8] = {
        INPUT_A0_OFFSET,
        INPUT_A1_OFFSET,
        INPUT_A2_OFFSET,
        INPUT_A3_OFFSET,
        INPUT_A4_OFFSET,
        INPUT_A5_OFFSET,
        INPUT_A6_OFFSET,
        INPUT_A7_OFFSET
    };

    uint64_t input_b_addr[8] = {
        INPUT_B0_OFFSET,
        INPUT_B1_OFFSET,
        INPUT_B2_OFFSET,
        INPUT_B3_OFFSET,
        INPUT_B4_OFFSET,
        INPUT_B5_OFFSET,
        INPUT_B6_OFFSET,
        INPUT_B7_OFFSET
    };

    // Write vector A inputs
    rc = fpga_pci_write_burst(pci_bar_handle, INPUT_A0_OFFSET, (uint32_t *) a, 2);
    rc = fpga_pci_write_burst(pci_bar_handle, INPUT_B0_OFFSET, (uint32_t *) b, 2);

    // Read the written inputs
    printf("Inputs written into FPGA:\n");
    uint32_t *intermediateA = malloc(sizeof(uint32_t));
    uint32_t *intermediateB = malloc(sizeof(uint32_t));
    for (int i = 0; i < 8; i++) {
        rc = fpga_pci_peek(pci_bar_handle, input_a_addr[i], intermediateA);
        rc = fpga_pci_peek(pci_bar_handle, input_b_addr[i], intermediateB);
        fail_on(rc, out, "Unable to read from FPGA");

        printf("A: %d, B: %d\n", (int) *intermediateA, (int) *intermediateB);
    }
    free(intermediateA);
    free(intermediateB);

    // Read result
    rc = fpga_pci_peek(pci_bar_handle, OUTPUT_OFFSET, result);
    fail_on(rc, out, "Unable to read from FPGA");

out:
    if (pci_bar_handle >= 0) {
        rc = fpga_pci_detach(pci_bar_handle);
        if (rc) {
            printf("Failure while detaching from FPGA.\n");
        }
    }

    return (rc != 0 ? 1 : 0);
}


int check_afi_ready(int slot_id) {
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

pci_bar_handle_t attach_fpga() {
    int rc;
    pci_bar_handle_t pci_bar_handle = PCI_BAR_HANDLE_INIT;

    rc = fpga_pci_attach(0, FPGA_APP_PF, APP_PF_BAR4, 0, &pci_bar_handle);
    fail_on(rc, out, "Unable to attach to the AFI on slot id %d", 0);

    return pci_bar_handle;
out:
    return 1;
}

