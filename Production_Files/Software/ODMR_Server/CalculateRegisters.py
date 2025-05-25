#%% This is from the ADF4351 datasheet, page 18
# https://www.analog.com/media/en/technical-documentation/data-sheets/adf4351.pdf
from math import gcd          # add this

def adf4351_registers(freq_hz, ref_hz=30e6, chan_spacing=100e3):
    f_pfd = ref_hz
    vco   = freq_hz
    div   = 0
    while vco < 2.2e9 and div < 7:
        vco *= 2; div += 1

    INT  = int(vco // f_pfd)
    MOD  = int(round(f_pfd / chan_spacing))
    FRAC = int(round((vco - INT * f_pfd) / chan_spacing))

    if FRAC == 0:           # integer-N
        MOD = 2
    else:
        from math import gcd
        g = gcd(FRAC, MOD)
        FRAC //= g
        MOD  //= g

    PHASE = 1
    reg = [0]*6
    reg[0] = (INT << 15) | (FRAC << 3)
    reg[1] = (1 << 27) | (PHASE << 15) | (MOD << 3) | 0x1
    reg[2] = 0x004E42
    reg[3] = 0x0004B3
    reg[4] = (0x009F003C & ~(0x7 << 20)) | (div << 20)
    reg[5] = 0x00580005
    return [f'0x{r:08X}' for r in reg]

# %%

# %%
adf4351_registers(1800000000)


# %%
