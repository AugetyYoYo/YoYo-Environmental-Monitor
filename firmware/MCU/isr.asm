        CSEG    AT  0123H           ;RTC_ISR
        JMP     RTC_ISR
        CSEG    AT  012BH           ;P0INT_VECTOR
        JMP     P0INT_ISR
        CSEG    AT  0133H           ;P1INT_VECTOR
        JMP     P1INT_ISR
        CSEG    AT  013BH           ;P2INT_VECTOR
        JMP     P2INT_ISR
        CSEG    AT  0143H           ;P3INT_VECTOR
        JMP     P3INT_ISR
        CSEG    AT  014BH           ;P4INT_VECTOR
        JMP     P4INT_ISR
        CSEG    AT  0153H           ;P5INT_VECTOR
        JMP     P5INT_ISR
        CSEG    AT  015BH           ;P6INT_VECTOR
        JMP     P6INT_ISR
        CSEG    AT  0163H           ;P7INT_VECTOR
        JMP     P7INT_ISR
        CSEG    AT  016BH           ;P8INT_VECTOR
        JMP     P8INT_ISR
        CSEG    AT  0173H           ;P9INT_VECTOR
        JMP     P9INT_ISR
        CSEG    AT  017BH           ;M2MDMA_VECTOR
        JMP     M2MDMA_ISR
        CSEG    AT  0183H           ;ADCDMA_VECTOR
        JMP     ADCDMA_ISR
        CSEG    AT  018BH           ;SPIDMA_VECTOR
        JMP     SPIDMA_ISR
        CSEG    AT  0193H           ;U1TXDMA_VECTOR
        JMP     U1TXDMA_ISR
        CSEG    AT  019BH           ;U1RXDMA_VECTOR
        JMP     U1RXDMA_ISR
        CSEG    AT  01A3H           ;U2TXDMA_VECTOR
        JMP     U2TXDMA_ISR
        CSEG    AT  01ABH           ;U2RXDMA_VECTOR
        JMP     U2RXDMA_ISR
        CSEG    AT  01B3H           ;U3TXDMA_VECTOR
        JMP     U3TXDMA_ISR
        CSEG    AT  01BBH           ;U3RXDMA_VECTOR
        JMP     U3RXDMA_ISR
        CSEG    AT  01C3H           ;U4TXDMA_VECTOR
        JMP     U4TXDMA_ISR
        CSEG    AT  01CBH           ;U4RXDMA_VECTOR
        JMP     U4RXDMA_ISR
        CSEG    AT  01D3H           ;LCMDMA_VECTOR
        JMP     LCMDMA_ISR
        CSEG    AT  01DBH           ;LCMIF_VECTOR
        JMP     LCMIF_ISR
        CSEG    AT  021BH           ;TIMER11_VECTOR
        JMP     TIMER11_ISR

RTC_ISR:
P0INT_ISR:
P1INT_ISR:
P2INT_ISR:
P3INT_ISR:
P4INT_ISR:
P5INT_ISR:
P6INT_ISR:
P7INT_ISR:
P8INT_ISR:
P9INT_ISR:
M2MDMA_ISR:
ADCDMA_ISR:
SPIDMA_ISR:
U1TXDMA_ISR:
U1RXDMA_ISR:
U2TXDMA_ISR:
U2RXDMA_ISR:
U3TXDMA_ISR:
U3RXDMA_ISR:
U4TXDMA_ISR:
U4RXDMA_ISR:
LCMDMA_ISR:
LCMIF_ISR:
TIMER11_ISR:
        JMP     006BH

        END
