// do not use in real os. it's unfinished.
#include "include/nyxis.h"
#include "include/memory.h"

// ======================================================
// AHCI DEFINES
// ======================================================

#define AHCI_BASE 0x400000

#define HBA_PORT_DET_PRESENT 3
#define HBA_PORT_IPM_ACTIVE  1

#define SATA_SIG_ATA 0x00000101

#define ATA_CMD_READ_DMA_EX 0x25

#define HBA_PX_CMD_ST   (1 << 0)
#define HBA_PX_CMD_FRE  (1 << 4)
#define HBA_PX_CMD_FR   (1 << 14)
#define HBA_PX_CMD_CR   (1 << 15)

#define FIS_TYPE_REG_H2D 0x27

// ======================================================
// AHCI STRUCTURES
// ======================================================

typedef volatile struct {
    u32 clb;
    u32 clbu;
    u32 fb;
    u32 fbu;
    u32 is;
    u32 ie;
    u32 cmd;
    u32 rsv0;
    u32 tfd;
    u32 sig;
    u32 ssts;
    u32 sctl;
    u32 serr;
    u32 sact;
    u32 ci;
    u32 sntf;
    u32 fbs;
    u32 rsv1[11];
    u32 vendor[4];
} HBA_PORT;

typedef volatile struct {
    u32 cap;
    u32 ghc;
    u32 is;
    u32 pi;
    u32 vs;
    u32 ccc_ctl;
    u32 ccc_pts;
    u32 em_loc;
    u32 em_ctl;
    u32 cap2;
    u32 bohc;

    u8  rsv[0xA0 - 0x2C];
    u8  vendor[0x100 - 0xA0];

    HBA_PORT ports[32];
} HBA_MEM;

typedef struct {
    u8  cfis[64];
    u8  acmd[16];
    u8  rsv[48];
} HBA_CMD_TBL;

typedef struct {
    u16 flags;
    u16 prdtl;
    u32 prdbc;
    u32 ctba;
    u32 ctbau;
    u32 rsv1[4];
} HBA_CMD_HEADER;

typedef struct {
    u8 fis_type;
    u8 pmport : 4;
    u8 rsv0   : 3;
    u8 c      : 1;

    u8 command;
    u8 featurel;

    u8 lba0;
    u8 lba1;
    u8 lba2;
    u8 device;

    u8 lba3;
    u8 lba4;
    u8 lba5;
    u8 featureh;

    u8 countl;
    u8 counth;
    u8 icc;
    u8 control;

    u8 rsv1[4];
} FIS_REG_H2D;

// ======================================================
// GLOBAL
// ======================================================

static volatile HBA_MEM* AhciBase = (HBA_MEM*)AHCI_BASE;

// ======================================================
// PORT HELPERS
// ======================================================

static void AhciStopCmd(HBA_PORT* Port) {
    Port->cmd &= ~HBA_PX_CMD_ST;
    Port->cmd &= ~HBA_PX_CMD_FRE;

    while (1) {
        if (!(Port->cmd & HBA_PX_CMD_FR) &&
            !(Port->cmd & HBA_PX_CMD_CR)) {
            break;
        }
    }
}

static void AhciStartCmd(HBA_PORT* Port) {
    while (Port->cmd & HBA_PX_CMD_CR);

    Port->cmd |= HBA_PX_CMD_FRE;
    Port->cmd |= HBA_PX_CMD_ST;
}

static i32 AhciCheckType(HBA_PORT* Port) {
    u32 ssts = Port->ssts;

    u8 ipm = (ssts >> 8) & 0x0F;
    u8 det = ssts & 0x0F;

    if (det != HBA_PORT_DET_PRESENT)
        return 0;

    if (ipm != HBA_PORT_IPM_ACTIVE)
        return 0;

    switch (Port->sig) {
        case SATA_SIG_ATA:
            return 1;
    }

    return 0;
}

// ======================================================
// FIND SATA PORT
// ======================================================

static HBA_PORT* AhciFindPort(void) {
    u32 pi = AhciBase->pi;

    for (i32 i = 0; i < 32; i++) {
        if (pi & 1) {
            if (AhciCheckType(&AhciBase->ports[i])) {
                return &AhciBase->ports[i];
            }
        }

        pi >>= 1;
    }

    return nNULL;
}

// ======================================================
// PORT INIT
// ======================================================

static void AhciPortRebase(HBA_PORT* Port, i32 PortNo) {
    AhciStopCmd(Port);

    Port->clb  = AHCI_BASE + (PortNo << 10);
    Port->clbu = 0;

    memset((void*)(usize)Port->clb, 0, 1024);

    Port->fb  = AHCI_BASE + (32 << 10) + (PortNo << 8);
    Port->fbu = 0;

    memset((void*)(usize)Port->fb, 0, 256);

    HBA_CMD_HEADER* CmdHeader =
        (HBA_CMD_HEADER*)(usize)Port->clb;

    for (i32 i = 0; i < 32; i++) {
        CmdHeader[i].prdtl = 1;

        CmdHeader[i].ctba =
            AHCI_BASE + (40 << 10) + (PortNo << 13) + (i << 8);

        CmdHeader[i].ctbau = 0;

        memset(
            (void*)(usize)CmdHeader[i].ctba,
            0,
            256
        );
    }

    AhciStartCmd(Port);
}

// ======================================================
// READ SECTOR
// ======================================================

i32 AhciRead(
    HBA_PORT* Port,
    u64 StartLba,
    u32 SectorCount,
    void* Buffer
) {
    Port->is = (u32)-1;

    HBA_CMD_HEADER* CmdHeader =
        (HBA_CMD_HEADER*)(usize)Port->clb;

    CmdHeader->flags =
        (sizeof(FIS_REG_H2D) / sizeof(u32)) | (1 << 6);

    CmdHeader->prdtl = 1;

    HBA_CMD_TBL* CmdTbl =
        (HBA_CMD_TBL*)(usize)CmdHeader->ctba;

    memset(CmdTbl, 0, sizeof(HBA_CMD_TBL));

    // ==================================================
    // PRDT
    // ==================================================

    u32* Prdt = (u32*)((u8*)CmdTbl + 0x80);

    Prdt[0] = (u32)(usize)Buffer;
    Prdt[1] = 0;
    Prdt[2] = (SectorCount << 9) - 1;
    Prdt[3] = 1;

    // ==================================================
    // FIS
    // ==================================================

    FIS_REG_H2D* Fis =
        (FIS_REG_H2D*)(&CmdTbl->cfis);

    Fis->fis_type = FIS_TYPE_REG_H2D;
    Fis->c = 1;

    Fis->command = ATA_CMD_READ_DMA_EX;

    Fis->lba0 = (u8)StartLba;
    Fis->lba1 = (u8)(StartLba >> 8);
    Fis->lba2 = (u8)(StartLba >> 16);
    Fis->lba3 = (u8)(StartLba >> 24);
    Fis->lba4 = (u8)(StartLba >> 32);
    Fis->lba5 = (u8)(StartLba >> 40);

    Fis->device = 1 << 6;

    Fis->countl = SectorCount & 0xFF;
    Fis->counth = (SectorCount >> 8) & 0xFF;

    // ==================================================
    // WAIT
    // ==================================================

    while ((Port->tfd & (0x80 | 0x08)));

    // ISSUE COMMAND

    Port->ci = 1;

    while (1) {
        if (!(Port->ci & 1))
            break;

        if (Port->is & (1 << 30))
            return 0;
    }

    if (Port->is & (1 << 30))
        return 0;

    return 1;
}

// ======================================================
// INIT
// ======================================================

void AhciInit(void) {
    HBA_PORT* Port = AhciFindPort();

    if (!Port) {
        printk("No SATA drive found\n");
        return;
    }

    printk("SATA drive detected\n");

    AhciPortRebase(Port, 0);

    static u8 Buffer[512];

    if (AhciRead(Port, 0, 1, Buffer)) {
        printk("Read success\n");

        for (i32 i = 0; i < 16; i++) {
            printk("%x ", Buffer[i]);
        }

        printk("\n");
    } else {
        printk("Read failed\n");
    }
}
