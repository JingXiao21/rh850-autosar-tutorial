"""Build review aids from manually checked HW-E facts; never accesses target MMIO.

Run without arguments to write JSON/CSV; --check verifies published files and
arithmetic. This is not a driver, an SVD, a silicon test, or an exhaustive map.
"""
from pathlib import Path
import csv
import io
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
B = 0xFFD20000
rows = []


def add(name, address, width, mode, access, pages, note=""):
    assert width in (8, 16, 32) and address % (width // 8) == 0
    rows.append(dict(name=name, address=f"0x{address:08X}", width_bits=width,
                     mode=mode, access=access, hw_e_pages=pages, note=note))


# CAN registers with shared addresses; fields can still differ by interface mode.
for name, off, access, pages, note in [
    ("GCFG",0x84,"RW","949-950","DCS bit4; 0=40MHz, 1=16MHz; FD-only fields exist"),
    ("GCTR",0x88,"RW","951-953","GMDC bits1:0; GSLPR bit2; configure in global reset"),
    ("GSTS",0x8C,"RO","954-955","GRAMINIT bit3; mode status bits2:0"),
    ("GERFL",0x90,"mixed","956-957","W0C and summary flags; mask undefined reserved reads"),
    ("GAFLECTR",0x98,"RW","963","AFLDAE bit8; AFLPN bits4:0; page0..11"),
    ("GAFLCFG0",0x9C,"RW","964","RNC0 31:24; RNC1 23:16; RNC2 15:8"),
    ("RMNB",0xA4,"RW","972","NRXMB 7:0; FD RMPLS 9:8; reference NRXMB=0"),
    ("GRMCFG",0x4FC,"RW","920","RCMC bit0; change only in global reset before other config"),
    ("CANFDMDR",0x8000,"RO","1056","FDMDR bit0 reports interface mode"),
]:
    add(name,B+off,32,"CAN_BOTH",access,pages,note)
add("GFDCFG",B+0x474,32,"CAN_FD","RW","962","RPED bit0; TSCCFG 9:8; no ISO selector defined here")
for m in range(3):
    add(f"C{m}CFG",B+16*m,32,"CAN_CLASSIC","RW","803-804","physical values minus1; channel reset/halt restrictions")
    add(f"C{m}NCFG",B+16*m,32,"CAN_FD","RW","921-922","not the same layout as Classic CFG")
    for name,off,access,page in [("CTR",4,"RW","923-925"),("STS",8,"mixed","928-929"),("ERFL",12,"W0C","930-934")]:
        add(f"C{m}{name}",B+16*m+off,32,"CAN_BOTH",access,page,"FD pages cited; Classic common fields only")
    add(f"C{m}DCFG",B+0x500+32*m,32,"CAN_FD","RW","935-936","overlaps Classic AFL window")
    add(f"C{m}FDCFG",B+0x504+32*m,32,"CAN_FD","RW","938-940","TDC needs board timing; FDOE is not RCMC")
    add(f"TXQCC{m}",B+0x3A0+4*m,32,"CAN_BOTH","RW","1039-1040","reference disabled")
    add(f"THLCC{m}",B+0x400+4*m,32,"CAN_BOTH","RW","1044-1045","reference disabled")
for x in range(8):
    for name,off,access,page,note in [
        ("RFCC",0xB8,"RW","979-980","RFE bit0; enable separately after global operating"),
        ("RFSTS",0xD8,"mixed","981-982","RFEMP0 RFFLL1 RO; RFMLT2 RFIF3 W0C; RFMC15:8 RO"),
        ("RFPCTR",0xF8,"WO","983","write0xFF only while enabled and nonempty; pops a message"),
    ]:
        add(f"{name}{x}",B+off+4*x,32,"CAN_BOTH",access,page,note)
for k in range(9):
    add(f"CFCC{k}",B+0x118+4*k,32,"CAN_BOTH","RW","990-994","common FIFO; reference disabled; not RFCC")
for p in range(48):
    add(f"TMC{p}",B+0x250+p,8,"CAN_BOTH","command","1018-1019","TMTR0/TMTAR1 write1 requests; TMOM2")
    add(f"TMSTS{p}",B+0x2D0+p,8,"CAN_BOTH","mixed","1020-1021","TMTRF2:1 result; write0 after consuming; request clear alone is not success")
for y in range(2):
    add(f"TMIEC{y}",B+0x390+4*y,32,"CAN_BOTH","RW","1036-1037","y1 only low16 implemented")
for mode,afl,rx,rxstep,tx,txstep,words,pages in [
    ("CAN_CLASSIC",0x500,0xE00,0x10,0x1000,0x10,2,"797-802; 17.3"),
    ("CAN_FD",0x1000,0x3000,0x80,0x4000,0x20,5,"963-971; 984-989; 1022-1028"),
]:
    for j in range(16):
        for name,off in [("GAFLID",0),("GAFLM",4),("GAFLP0",8),("GAFLP1",12)]:
            add(f"{name}_{j}",B+afl+16*j+off,32,mode,"RW",pages,"page window slot, not absolute rule index; write only AFLDAE=1")
    for x in range(8):
        add(f"RFID{x}",B+rx+rxstep*x,32,mode,"RO",pages,"allocated enabled nonempty FIFO only")
        add(f"RFPTR{x}",B+rx+rxstep*x+4,32,mode,"RO",pages,"DLC31:28; label27:16; timestamp15:0")
        if mode=="CAN_FD":
            add(f"RFFDSTS{x}",B+rx+rxstep*x+8,32,mode,"RO",pages,"FDF2 BRS1 ESI0")
        for d in range(16 if mode=="CAN_FD" else 2):
            add(f"RFDF{d}_{x}",B+rx+rxstep*x+(12 if mode=="CAN_FD" else 8)+4*d,32,mode,"RO",pages,"read only allocated payload words for current frame before pop")
    for p in range(48):
        add(f"TMID{p}",B+tx+txstep*p,32,mode,"RW",pages,"standalone Tx buffer; ownership required; merge/queue can repurpose")
        add(f"TMPTR{p}",B+tx+txstep*p+4,32,mode,"RW",pages,"DLC31:28; label23:16")
        if mode=="CAN_FD":
            add(f"TMFDCTR{p}",B+tx+txstep*p+8,32,mode,"RW",pages,"FDF2 BRS1 ESI0")
        for d in range(words):
            add(f"TMDF{d}_{p}",B+tx+txstep*p+(12 if mode=="CAN_FD" else 8)+4*d,32,mode,"RW",pages,"byte0 in low8; never overwrite pending transmission")

port = [
    ("P",0,16,"RW","113"),("PSR",4,32,"RW","115"),
    ("PPR",12,16,"RO","112"),("PM",16,16,"RW","104"),
    ("PMC",20,16,"RW","101"),("PFC",24,16,"RW","107"),
    ("PFCE",28,16,"RW","108"),("PMSR",32,32,"RW","105"),
    ("PMCSR",36,32,"RW","102"),("PFCAE",40,16,"RW","109"),
    ("PINV",48,32,"RW","116"),("PIBC",0x4000,16,"RW","106"),
    ("PBDC",0x4004,16,"RW","111"),("PIPC",0x4008,16,"RW","103;131"),
    ("PU",0x400C,16,"RW","117"),("PD",0x4010,16,"RW","118"),
    ("PODC",0x4014,32,"RW","119"),("PDSC",0x4018,32,"RW","121"),
    ("PUCC",0x4028,32,"RW","122"),("PISA",0x402C,16,"RW","123"),
    ("PODCE",0x403C,32,"RW","120"),
]
for n in range(2,6):
    for name,off,width,access,page in port:
        if name=="PISA" and n==5:
            continue
        add(f"{name}{n}",0xFFC10000+64*n+off,width,"SYSTEM",access,page,"CAN candidate port group; only implemented pin bits; preserve other owners")
for n,base in [(0,0xFFDD8000),(1,0xFFDD9000)]:
    add(f"IC0CKSEL{n}",0xFFDD6000+4*n,16,"SYSTEM","RW","1557-1560","bit15=0 selects PCLK80MHz; configure stopped")
    for name,off,width,access in [("CMP",0,32,"RW"),("CNT",4,32,"RO"),("TO",8,8,"RW"),("TOE",12,8,"RW"),("TE",16,8,"RO"),("TS",20,8,"WO"),("TT",24,8,"WO"),("CTL",32,8,"RW")]:
        add(f"OSTM{n}{name}",base+off,width,"SYSTEM",access,"1551-1556","OS/Gpt unique owner; TS/TT commands, do not read-modify-write")
irq = [(74,"OSTM0","edge"),(75,"OSTM1","edge")]+[(183+i,name,"level") for i,name in enumerate(["CAN0ERR","CAN0REC_COMMON","CAN0TRX","CAN1ERR","CAN1REC_COMMON","CAN1TRX","CANGERR","CANGRECC_RXFIFO","CAN2ERR","CAN2REC_COMMON","CAN2TRX"])]
interrupts=[]
for n,name,kind in irq:
    address=(0xFFFEEA00 if n<32 else 0xFFFFB000)+2*n
    add(f"EIC{n}_{name}",address,16,"SYSTEM","mixed","267-268;283;285-286",f"{kind}; EIP3:0 EITB6 EIMK7 EIRF12 EICT15; OS ownership")
    interrupts.append(dict(source=name,ei_channel=n,eic_address=f"0x{address:08X}",table_offset=f"0x{4*n:03X}",type=kind))
for name,address in [("CLKD2DIV",0xFFF88810),("CLKD2STAT",0xFFF88814),("CKSC2C",0xFFF89080),("CKSC2S",0xFFF89088)]:
    add(name,address,32,"SYSTEM","RO" if name.endswith(("STAT","S")) else "RW","471-477","external clock output only; not a CPU PLL control")
for name,off,access in [("WDTE",0,"RW"),("EVAC",4,"RW"),("REF",8,"RO"),("MD",12,"RW")]:
    add("WDTA0"+name,0xFFD74000+off,8,"SYSTEM",access,"1527-1532","startup/window/VAC restrictions apply; not a general-purpose timer")
add("OPBT0",0xFFCD0030,32,"SYSTEM","RO","2884","read-only mirror; not an MMIO option-byte programming interface")
for name,address,access,page,note in [
    ("RESF",0xFFF81000,"RO","421-422","capture reset causes before clear; bit6 undefined"),
    ("RESFC",0xFFF81008,"WO","423","write1 clears matching cause; not CAN W0C semantics"),
    ("STAC_DTSRAM",0xFFF81320,"RW","427","application reset RAM policy"),
    ("STAC_GRAM",0xFFF81420,"RW","428","application reset RAM policy"),
    ("STAC_LM0",0xFFF81520,"RW","429","system reset except CVM and application reset LRAM policy"),
    ("STAC_LM10",0xFFF81E20,"RW","430","application reset CSIH RAM policy"),
]:
    add(name,address,32,"SYSTEM",access,page,note)

pins=[(0,"P2_0",1,"P2_1",1),(0,"P3_7",3,"P3_8",3),(0,"P4_5",3,"P4_6",3),(1,"P2_2",1,"P2_3",1),(1,"P3_12",3,"P3_13",3),(1,"P4_2",1,"P4_3",1),(2,"P5_6",1,"P5_5",6)]
profiles={
    "A_CLASSIC_500K":dict(rcmc=0,fcan_hz=40000000,divider=4,tseg1=15,tseg2=4,sjw=3,cfg="0x023E0003",rfcc0_poll_disabled="0x00001200",rfcc0_poll_enabled="0x00001201",rfcc0_irq_disabled="0x00001202",rfcc0_irq_enabled="0x00001203",fifo_payload_bytes=8,fifo_depth=8),
    "B_FD_500K_1M_16B":dict(rcmc=1,fcan_hz=40000000,divider=2,nominal_tseg1=31,nominal_tseg2=8,nominal_sjw=4,data_tseg1=15,data_tseg2=4,data_sjw=3,ncfg="0x071E1801",dcfg="0x023E0001",rfcc0_poll_disabled="0x00001220",rfcc0_poll_enabled="0x00001221",rfcc0_irq_disabled="0x00001222",rfcc0_irq_enabled="0x00001223",fifo_payload_bytes=16,fifo_depth=8,tdc="UNBOUND: board propagation timing and supplier policy required"),
}
data=dict(schema_version=1,target="R7F701381 / RH850-P1M-E / 100-pin DPS",date="2026-10-01",source="R01UH0585EJ0120 Rev1.20",source_sha256="aaea89a7f5d9b029776945868d21728465d372223c41db05cbd728a0499a6e34",purpose="Reviewed address aid; NOT an executable write sequence, complete SVD, or board-verified configuration",mode_policy="Use SYSTEM + CAN_BOTH + exactly one of CAN_CLASSIC/CAN_FD. Same addresses can mean different things in different modes.",width_policy="width_bits is the selected documented access width; narrower aliases are intentionally omitted",registers=rows,interrupts=interrupts,pin_candidates=[dict(channel=c,rx=r,rx_alt=ra,tx=t,tx_alt=ta,pipc=0,hw_e_pages="151-154",board_binding="UNBOUND") for c,r,ra,t,ta in pins],reference_profiles=profiles,rx_id_example=["0x4E0","0x7DF","0x014","0x0FD","0x3F2","0x4F0","0x07B"])

# Static consistency checks against separately written worked examples.
lookup={(r['mode'],r['name']):r for r in rows}
assert len(lookup)==len(rows)
for mode in ("CAN_CLASSIC","CAN_FD"):
    selected=[r for r in rows if r['mode'] in ("SYSTEM","CAN_BOTH",mode)]
    assert len({r['address'] for r in selected})==len(selected)
for mode,name,address,width in [
    ("CAN_FD","C2DCFG","0xFFD20540",32),
    ("CAN_CLASSIC","GAFLID_0","0xFFD20500",32),
    ("CAN_FD","GAFLID_0","0xFFD21000",32),
    ("CAN_CLASSIC","TMID0","0xFFD21000",32),
    ("CAN_FD","TMID16","0xFFD24200",32),
    ("CAN_BOTH","TMC47","0xFFD2027F",8),
    ("CAN_BOTH","TMSTS47","0xFFD202FF",8),
    ("SYSTEM","PINV2","0xFFC100B0",32),
    ("SYSTEM","PIPC2","0xFFC14088",16),
    ("SYSTEM","EIC190_CANGRECC_RXFIFO","0xFFFFB17C",16),
    ("SYSTEM","OSTM1TT","0xFFDD9018",8),
]:
    r=lookup[mode,name];assert (r['address'],r['width_bits'])==(address,width)
assert (3 | (14<<16) | (3<<20) | (2<<24))==int(profiles['A_CLASSIC_500K']['cfg'],16)
assert (1 | (3<<11) | (30<<16) | (7<<24))==int(profiles['B_FD_500K_1M_16B']['ncfg'],16)
assert (1 | (14<<16) | (3<<20) | (2<<24))==int(profiles['B_FD_500K_1M_16B']['dcfg'],16)
assert 40000000//(4*(1+15+4))==500000
assert 40000000//(2*(1+31+8))==500000
assert 40000000//(2*(1+15+4))==1000000
assert 80000000//1000-1==0x1387F
for p in profiles.values():
    enabled=int(p['rfcc0_irq_enabled'],16)
    assert enabled & 3 == 3 and (enabled>>8)&7==2 and enabled & (1<<12)
    assert ((enabled>>4)&7)==(2 if p['rcmc'] else 0)

json_text=json.dumps(data,ensure_ascii=False,indent=2)+"\n"
out=io.StringIO(newline="")
writer=csv.DictWriter(out,fieldnames=list(rows[0]),lineterminator="\n")
writer.writeheader();writer.writerows(rows)
outputs={ROOT/'docs/hardware-registers.json':json_text,ROOT/'docs/hardware-registers.csv':out.getvalue()}
if '--check' in sys.argv:
    for path,expected in outputs.items():
        assert path.read_text(encoding='utf-8')==expected,f"stale index: {path}"
    print(f"PASS: {len(rows)} register entries; 2 exclusive CAN maps; widths/alignment, worked addresses, timing and FIFO profiles; published JSON/CSV match source.")
    print("Scope: static documentation consistency only; no silicon, MCAL, OS ABI or bus validation.")
else:
    for path,contents in outputs.items():
        path.write_text(contents,encoding='utf-8',newline='\n')
    print(f"Wrote {len(rows)} register entries to docs/hardware-registers.json and .csv")
