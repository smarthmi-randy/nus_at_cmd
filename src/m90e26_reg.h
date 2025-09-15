#ifndef M90E26_REG_H_
#define M90E26_REG_H_

#define TOTAL_REG_NUM   21
enum ATM90E26_ENG_REGSTERS {
    APENG = 0x40,
    ANENG = 0x41,
    ATENG = 0x42,
    RPENG = 0x43,
    RNENG = 0x44,
    RTENG = 0x45,
    ENSTAT = 0x46,
    IRMS = 0x48,
    URMS = 0x49,
    PMEAN = 0x4A,
    QMEAN = 0x4B,
    FREQ = 0x4C,
    POWERF = 0x4D,
    PANG = 0x4E,
    SMEAN = 0x4F,
    IRMS2 = 0x68,
    PMEAN2 = 0x6A,
    QMEAN2 = 0x6B,
    POWERF2 = 0x6D,
    PANG2 = 0x6E,
    SMEAN2 = 0x6F
};

#endif /* M90E26_REG_H_ */