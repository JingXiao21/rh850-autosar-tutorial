"""
emit_bsw.py - emits the BSW configuration files from the ECUC JSON: Com, PduR, CanIf, Can, EcuM, BswM, Mcu, Port, Dio, Adc.

[Educational Implementation]
Real AUTOSAR counterpart: the BSW module configurators (one ECUC container tree per module -> <Module>_Cfg.h/.c). Each
module consists of hand-written code (the driver) plus these constant tables; the module headers define the table layout
(Com_Types.h, PduR.h, CanIf.h, Can.h, ...). The generator also checks that the layers agree with each other (CAN id of
a ComIPdu = CanIf L-PDU = Can hardware object, DLC = I-PDU length, PduR path joins the right pair), which in real projects
is the job of the cross-module validation of the configuration tool.
"""
import re
from collections import OrderedDict

from emit_util import banner, W, CFG_PHASE
from arxml_model import id_name, parse_c_int

PREFIX_HEADER = OrderedDict([("Can_", "Can.h"), ("CanIf_", "CanIf.h"), ("Com_", "Com.h"), ("PduR_", "PduR.h"),
                             ("EcuM_", "EcuM.h"), ("BswM_", "BswM.h"), ("Det_", "Det.h"), ("SchM_", "SchM.h"),
                             ("IoHwAb_", "IoHwAb.h"), ("Mcu_", "Mcu.h"), ("McuConf_", "Mcu.h"), ("Port_", "Port.h"),
                             ("Dio_", "Dio.h"), ("Adc_", "Adc.h"), ("Trace_", "Trace.h"), ("Rte_", "Rte.h"),
                             ("RTE_", "Rte.h"), ("ComConf_", "Com.h"), ("EcuMConf_", "EcuM.h"), ("CAN_CS", "CanIf.h"),
                             ("ECUM_", "EcuM.h")])
STD_RETURNING = {"EcuM_RequestRUN", "EcuM_ReleaseRUN", "Rte_Start", "Rte_Stop", "CanIf_SetControllerMode", "Mcu_InitClock",
                 "Mcu_DistributePllClock"}


def headers_for(exprs):
    hs = []
    for e in exprs:
        for ident in re.findall(r"[A-Za-z_][A-Za-z_0-9]*", e):
            for pre, h in PREFIX_HEADER.items():
                if ident.startswith(pre) and h not in hs:
                    hs.append(h)
    return sorted(hs)


def returns_std(call):
    fn = re.match(r"\s*([A-Za-z_0-9]+)", call).group(1)
    return fn in STD_RETURNING or fn.startswith("Rte_Switch_")


def pin_number(pin):
    m = re.match(r"^P([A-K])(\d{1,2})$", pin)
    if not m:
        raise ValueError("bad pin name %r" % pin)
    return (ord(m.group(1)) - ord("A")) * 16 + int(m.group(2))


def hexs(v):
    return "0x%Xu" % parse_c_int(v)


# ------------------------------------------------------------------------------------------------ validation
def validate_bsw(ecu):
    ec = ecu.ecuc
    ew = ecu.name + ".ecuc.json"
    errs = []
    com, pr, ci, can = ec["Com"], ec["PduR"], ec["CanIf"], ec["Can"]
    ipdus = OrderedDict((p["name"], p) for p in com["ComIPdu"])
    rxp = OrderedDict((p["name"], p) for p in ci.get("CanIfRxPduCfg", []))
    txp = OrderedDict((p["name"], p) for p in ci.get("CanIfTxPduCfg", []))
    hoh = OrderedDict((h["name"], h) for h in can["CanHardwareObject"])
    paths = pr["PduRRoutingPath"]
    for p in paths:
        ip = ipdus.get(p["comIPdu"])
        if ip is None:
            errs.append("%s: PduRRoutingPath %s: unknown ComIPdu '%s'" % (ew, p["name"], p["comIPdu"]))
            continue
        if p["dir"] != ip["direction"]:
            errs.append("%s: PduRRoutingPath %s direction %s != ComIPdu %s direction %s" % (ew, p["name"], p["dir"], ip["name"], ip["direction"]))
        lp = (rxp if p["dir"] == "RX" else txp).get(p.get("canIfRxPdu" if p["dir"] == "RX" else "canIfTxPdu"))
        if lp is None:
            errs.append("%s: PduRRoutingPath %s: unknown CanIf %s PDU" % (ew, p["name"], p["dir"]))
            continue
        if parse_c_int(lp["canId"]) != parse_c_int(ip["canId"]):
            errs.append("%s: CAN id mismatch on path %s: CanIf L-PDU %s=0x%X, ComIPdu %s=0x%X" % (
                ew, p["name"], lp["name"], parse_c_int(lp["canId"]), ip["name"], parse_c_int(ip["canId"])))
        if lp["dlc"] != ip["lengthBytes"]:
            errs.append("%s: DLC mismatch on path %s: CanIf %d != Com lengthBytes %d" % (ew, p["name"], lp["dlc"], ip["lengthBytes"]))
        h = hoh.get(lp.get("hrh" if p["dir"] == "RX" else "hth"))
        if h is None:
            errs.append("%s: CanIf L-PDU %s refers to unknown CanHardwareObject" % (ew, lp["name"]))
        elif h["type"] != ("RECEIVE" if p["dir"] == "RX" else "TRANSMIT"):
            errs.append("%s: CanIf L-PDU %s uses HOH %s of type %s" % (ew, lp["name"], h["name"], h["type"]))
        elif parse_c_int(h["canId"]) != parse_c_int(lp["canId"]):
            errs.append("%s: HOH %s canId 0x%X != L-PDU %s canId 0x%X" % (ew, h["name"], parse_c_int(h["canId"]), lp["name"], parse_c_int(lp["canId"])))
    routed = set(p["comIPdu"] for p in paths)
    for n in ipdus:
        if n not in routed:
            errs.append("%s: ComIPdu %s has no PduRRoutingPath" % (ew, n))
    for n in rxp:
        if n not in set(p.get("canIfRxPdu") for p in paths):
            errs.append("%s: CanIfRxPduCfg %s is not routed by any PduRRoutingPath" % (ew, n))
    for n in txp:
        if n not in set(p.get("canIfTxPdu") for p in paths):
            errs.append("%s: CanIfTxPduCfg %s is not routed by any PduRRoutingPath" % (ew, n))
    for pin in ec.get("Port", {}).get("PortPin", []):
        try:
            pin_number(pin["pin"])
        except ValueError as e:
            errs.append("%s: Port %s: %s" % (ew, pin["name"], e))
    for d in ec.get("Dio", {}).get("DioChannel", []):
        if d["pin"] not in [p["pin"] for p in ec.get("Port", {}).get("PortPin", [])]:
            errs.append("%s: DioChannel %s uses pin %s which is not configured in Port" % (ew, d["name"], d["pin"]))
    groups = [g["name"] for g in com.get("ComIPduGroup", [])]
    if len(groups) > 8:
        errs.append("%s: at most 8 I-PDU groups supported" % ew)
    return errs


# ------------------------------------------------------------------------------------------------ helpers
def _cfg_h(ecu, mod, what, body_fn, extra_include=None):
    fn = "%s_Cfg.h" % mod
    w = W()
    w(banner(fn, ecu, what, CFG_PHASE).rstrip("\n"))
    g = fn.replace(".", "_").upper()
    w("#ifndef %s" % g)
    w("#define %s" % g)
    w()
    body_fn(w)
    w()
    w("#endif /* %s */" % g)
    return fn, w.text()


def _define(w, name, val, comment=""):
    w("#define %-52s %s%s" % (name, val, ("   /* %s */" % comment) if comment else ""))


# ------------------------------------------------------------------------------------------------ Com
def gen_com(ecu):
    ec = ecu.ecuc
    com = ec["Com"]
    ipdus = com["ComIPdu"]
    sigs = com["ComSignal"]
    groups = com.get("ComIPduGroup", [])
    ipdu_index = OrderedDict((p["name"], i) for i, p in enumerate(ipdus))
    tx_paths = [p for p in ec["PduR"]["PduRRoutingPath"] if p["dir"] == "TX"]
    out = OrderedDict()

    def h(w):
        _define(w, "COM_NUM_SIGNALS", "%du" % len(sigs))
        _define(w, "COM_NUM_IPDUS", "%du" % len(ipdus))
        _define(w, "COM_NUM_IPDU_GROUPS", "%du" % len(groups))
        w()
        w("/* ids = index in Com_Config.signals / Com_Config.ipdus (the handle PduR and the RTE use) */")
        for i, s in enumerate(sigs):
            _define(w, "ComConf_" + id_name("ComSignal", s["name"]), "%du" % i)
        for i, p in enumerate(ipdus):
            _define(w, "ComConf_" + id_name("ComIPdu", p["name"]), "%du" % i)
        for g in groups:
            _define(w, "ComConf_" + id_name("ComIPduGroup", g["name"]), "%du" % g["id"])
    fn, txt = _cfg_h(ecu, "Com", "Com ids and sizes of ECU %s. Signal id = index of the signal in Com_Config.signals (used by "
                     "Rte.c: Com_SendSignal(ComConf_ComSignal_X, ...)); I-PDU id = handle used by PduR and CanIf-side routing." % ecu.name, h)
    out[fn] = txt

    w = W()
    w(banner("Com_Cfg.c", ecu, "Com configuration tables of ECU %s: signal layout in the I-PDUs (bit position/size/type), I-PDU "
             "directions, transmit modes and periods, reception processing (IMMEDIATE = in the RX ISR, DEFERRED = in "
             "Com_MainFunctionRx) and the RTE notification callbacks. The table layout is defined by bsw/com/Com_Types.h; the real "
             "Com has far more (filters, update bits, deadline monitoring, signal groups)." % ecu.name, CFG_PHASE).rstrip("\n"))
    w('#include "Com.h"')
    w('#include "Rte_Cbk.h"   /* Rte_COMCbk_<signal> notification functions */')
    w('#include "MemMap.h"')
    w()
    w("/* ---- I-PDU buffers (section .bss.com): the shadow copy Com packs/unpacks signals into ---- */")
    for p in ipdus:
        w("static uint8 Com_Buf_%s[%d] MINI_VAR_COM_BUF;" % (p["name"], p["lengthBytes"]))
    w()
    w("/* ---- signals (ComSignal) ---- */")
    w("static const Com_SignalConfigType Com_Signals[COM_NUM_SIGNALS] MINI_CONST_CFG = {")
    for k, s in enumerate(sigs):
        n = s.get("notification")
        w("    { /* %s in %s */" % (s["name"], s["ipdu"]))
        w("      .bitPosition = %du, .bitSize = %du," % (s["bitPosition"], s["bitSize"]))
        w("      .type = COM_%s, .endianness = COM_%s_ENDIAN," % (s["type"], s["endianness"]))
        w("      .ipduId = ComConf_%s," % id_name("ComIPdu", s["ipdu"]))
        w("      .initValue = %su," % s["init"])
        w("      .transferProperty = COM_%s," % s.get("transfer", "PENDING"))
        w("      .rxNotification = %s }%s" % (n if n else "NULL_PTR", "," if k < len(sigs) - 1 else ""))
    w("};")
    w()
    w("/* ---- I-PDUs (ComIPdu): handle = index. TX: pdurPduId = PduR tx path index (see PduR_Cfg.c) ---- */")
    w("static const Com_IpduConfigType Com_Ipdus[COM_NUM_IPDUS] MINI_CONST_CFG = {")
    for k, p in enumerate(ipdus):
        tx = p["direction"] == "TX"
        w("    { /* %s, CAN id %s */" % (p["name"], p["canId"]))
        w("      .direction = COM_PDU_%s, .length = %du, .buffer = Com_Buf_%s," % (p["direction"], p["lengthBytes"], p["name"]))
        if tx:
            idx = [i for i, t in enumerate(tx_paths) if t["comIPdu"] == p["name"]][0]
            w("      .pdurPduId = %du,                   /* PduR tx path %s */" % (idx, tx_paths[idx]["name"]))
        else:
            w("      .pdurPduId = 0u,")
        grp = [g for g in groups if g["name"] == p["group"]][0]
        w("      .group = %du," % grp["id"])
        w("      .txMode = COM_TX_MODE_%s, .txPeriodMs = %du, .txOffsetMs = %du," % (p.get("txMode", "NONE") if tx else "NONE", p.get("txPeriodMs", 0), p.get("txOffsetMs", 0)))
        w("      .rxProcessing = COM_RX_%s }%s" % (p.get("rxProcessing", "IMMEDIATE") if not tx else "IMMEDIATE", "," if k < len(ipdus) - 1 else ""))
    w("};")
    w()
    w("const Com_ConfigType Com_Config = {")
    w("    .numSignals = COM_NUM_SIGNALS,")
    w("    .signals = Com_Signals,")
    w("    .numIpdus = COM_NUM_IPDUS,")
    w("    .ipdus = Com_Ipdus,")
    w("    .mainFunctionPeriodMs = %du" % com["mainFunctionPeriodMs"])
    w("};")
    out["Com_Cfg.c"] = w.text()
    return out


# ------------------------------------------------------------------------------------------------ PduR
def gen_pdur(ecu):
    ec = ecu.ecuc
    paths = ec["PduR"]["PduRRoutingPath"]
    rx = [p for p in paths if p["dir"] == "RX"]
    tx = [p for p in paths if p["dir"] == "TX"]
    out = OrderedDict()

    def h(w):
        _define(w, "PDUR_NUM_RX_PATHS", "%du" % len(rx))
        _define(w, "PDUR_NUM_TX_PATHS", "%du" % len(tx))
        w()
        w("/* path id = index in the rx / tx path table; this is the id CanIf stores as upperPduId and Com as pdurPduId */")
        for i, p in enumerate(rx):
            _define(w, "PduRConf_PduRRxPath_%s" % p["name"], "%du" % i)
        for i, p in enumerate(tx):
            _define(w, "PduRConf_PduRTxPath_%s" % p["name"], "%du" % i)
    fn, txt = _cfg_h(ecu, "PduR", "PDU router path ids of ECU %s. Routing is strictly 1:1 interface routing: one Com I-PDU <-> one CanIf L-PDU." % ecu.name, h)
    out[fn] = txt
    ipd = [p["name"] for p in ec["Com"]["ComIPdu"]]
    ctx = [p["name"] for p in ec["CanIf"].get("CanIfTxPduCfg", [])]
    w = W()
    w(banner("PduR_Cfg.c", ecu, "PDU router routing tables of ECU %s: per path which Com I-PDU is connected to which CanIf L-PDU. "
             "PduR itself is stateless glue; this table is its whole behaviour." % ecu.name, CFG_PHASE).rstrip("\n"))
    w('#include "PduR.h"')
    w('#include "Com.h"      /* ComConf_ComIPdu_* */')
    w('#include "CanIf.h"    /* CanIfConf_CanIfTxPduCfg_* */')
    w('#include "MemMap.h"')
    w()
    if tx:
        w("static const PduR_TxPathType PduR_TxPaths[PDUR_NUM_TX_PATHS] MINI_CONST_CFG = {")
        for k, p in enumerate(tx):
            w("    { .comTxPduId = ComConf_%s, .canIfTxPduId = CanIfConf_%s }%s   /* %s: Com -> CanIf */" % (
                id_name("ComIPdu", p["comIPdu"]), id_name("CanIfTxPduCfg", p["canIfTxPdu"]), "," if k < len(tx) - 1 else "", p["name"]))
        w("};")
        w()
    if rx:
        w("static const PduR_RxPathType PduR_RxPaths[PDUR_NUM_RX_PATHS] MINI_CONST_CFG = {")
        for k, p in enumerate(rx):
            w("    { .comRxPduId = ComConf_%s }%s   /* %s: CanIf -> Com */" % (
                id_name("ComIPdu", p["comIPdu"]), "," if k < len(rx) - 1 else "", p["name"]))
        w("};")
        w()
    w("const PduR_PBConfigType PduR_Config = {")
    w("    .numTxPaths = PDUR_NUM_TX_PATHS,")
    w("    .txPaths = %s," % ("PduR_TxPaths" if tx else "NULL_PTR"))
    w("    .numRxPaths = PDUR_NUM_RX_PATHS,")
    w("    .rxPaths = %s" % ("PduR_RxPaths" if rx else "NULL_PTR"))
    w("};")
    out["PduR_Cfg.c"] = w.text()
    return out


# ------------------------------------------------------------------------------------------------ CanIf
def gen_canif(ecu):
    ec = ecu.ecuc
    ci = ec["CanIf"]
    rxs, txs = ci.get("CanIfRxPduCfg", []), ci.get("CanIfTxPduCfg", [])
    hohs = ec["Can"]["CanHardwareObject"]
    hidx = dict((h["name"], i) for i, h in enumerate(hohs))
    paths = ec["PduR"]["PduRRoutingPath"]
    rxp = [p for p in paths if p["dir"] == "RX"]
    txp = [p for p in paths if p["dir"] == "TX"]
    out = OrderedDict()

    def h(w):
        _define(w, "CANIF_NUM_RX_PDUS", "%du" % len(rxs))
        _define(w, "CANIF_NUM_TX_PDUS", "%du" % len(txs))
        w()
        for i, p in enumerate(rxs):
            _define(w, "CanIfConf_" + id_name("CanIfRxPduCfg", p["name"]), "%du" % i)
        for i, p in enumerate(txs):
            _define(w, "CanIfConf_" + id_name("CanIfTxPduCfg", p["name"]), "%du" % i)
    fn, txt = _cfg_h(ecu, "CanIf", "CAN interface L-PDU ids of ECU %s (index in the rx / tx L-PDU tables)." % ecu.name, h)
    out[fn] = txt
    w = W()
    w(banner("CanIf_Cfg.c", ecu, "CAN interface L-PDU tables of ECU %s: CAN id <-> L-PDU <-> hardware object (HRH/HTH) <-> upper layer "
             "(PduR path). Reception does an exact CAN-id match on rxPdus[]; transmission is addressed by TxPduId = index in txPdus[]." % ecu.name,
             CFG_PHASE).rstrip("\n"))
    w('#include "CanIf.h"')
    w('#include "Can.h"      /* CanConf_CanHardwareObject_* */')
    w('#include "PduR_Cfg.h"')
    w('#include "MemMap.h"')
    w()
    if txs:
        w("static const CanIf_TxPduConfigType CanIf_TxPdus[CANIF_NUM_TX_PDUS] MINI_CONST_CFG = {")
        for k, p in enumerate(txs):
            path = [x for x in txp if x["canIfTxPdu"] == p["name"]][0]
            w("    { .canId = %s, .hth = CanConf_%s, .controller = 0u, .dlc = %du, .upperPduId = PduRConf_PduRTxPath_%s }%s" % (
                hexs(p["canId"]), id_name("CanHardwareObject", p["hth"]), p["dlc"], path["name"], "," if k < len(txs) - 1 else ""))
        w("};")
        w()
    if rxs:
        w("static const CanIf_RxPduConfigType CanIf_RxPdus[CANIF_NUM_RX_PDUS] MINI_CONST_CFG = {")
        for k, p in enumerate(rxs):
            path = [x for x in rxp if x["canIfRxPdu"] == p["name"]][0]
            w("    { .canId = %s, .hrh = CanConf_%s, .dlc = %du, .upperPduId = PduRConf_PduRRxPath_%s }%s" % (
                hexs(p["canId"]), id_name("CanHardwareObject", p["hrh"]), p["dlc"], path["name"], "," if k < len(rxs) - 1 else ""))
        w("};")
        w()
    w("const CanIf_ConfigType CanIf_Config = {")
    w("    .numTxPdus = CANIF_NUM_TX_PDUS,")
    w("    .txPdus = %s," % ("CanIf_TxPdus" if txs else "NULL_PTR"))
    w("    .numRxPdus = CANIF_NUM_RX_PDUS,")
    w("    .rxPdus = %s" % ("CanIf_RxPdus" if rxs else "NULL_PTR"))
    w("};")
    out["CanIf_Cfg.c"] = w.text()
    return out


# ------------------------------------------------------------------------------------------------ Can
def gen_can(ecu):
    ec = ecu.ecuc
    can = ec["Can"]
    hohs = can["CanHardwareObject"]
    out = OrderedDict()

    def h(w):
        _define(w, "CAN_NUM_HOH", "%du" % len(hohs))
        w()
        w("/* HRH and HTH ids share one number space (index in Can_Config.hoh) */")
        for i, x in enumerate(hohs):
            _define(w, "CanConf_" + id_name("CanHardwareObject", x["name"]), "%du" % i, x["type"])
    fn, txt = _cfg_h(ecu, "Can", "CAN driver hardware object ids of ECU %s." % ecu.name, h)
    out[fn] = txt
    w = W()
    ctl = can["CanController"]
    w(banner("Can_Cfg.c", ecu, "CAN driver configuration of ECU %s: controller %s (%s), %d bit/s, RX by %s, and the hardware objects "
             "(HRH = receive filter, HTH = transmit buffer)." % (ecu.name, ctl["name"], ctl["hardware"], ctl["baudrate"],
                                                                 "interrupt (Cat2 ISR Isr_CanRx)" if ctl["rxInterrupt"] else "polling"), CFG_PHASE).rstrip("\n"))
    w('#include "Can.h"')
    w('#include "MemMap.h"')
    w()
    w("static const Can_HohConfigType Can_Hohs[CAN_NUM_HOH] MINI_CONST_CFG = {")
    for k, x in enumerate(hohs):
        w("    { .hoh = CanConf_%s, .type = CAN_HOH_%s, .canId = %s, .filterMask = %s, .controller = 0u }%s" % (
            id_name("CanHardwareObject", x["name"]), "RECEIVE" if x["type"] == "RECEIVE" else "TRANSMIT",
            hexs(x["canId"]), hexs(x["filterMask"]), "," if k < len(hohs) - 1 else ""))
    w("};")
    w()
    w("const Can_ConfigType Can_Config = {")
    w("    .baudrate = %du," % ctl["baudrate"])
    w("    .rxInterrupt = %s," % ("TRUE" if ctl["rxInterrupt"] else "FALSE"))
    w("    .numHoh = CAN_NUM_HOH,")
    w("    .hoh = Can_Hohs")
    w("};")
    out["Can_Cfg.c"] = w.text()
    return out


# ------------------------------------------------------------------------------------------------ EcuM
def gen_ecum(ecu):
    ec = ecu.ecuc
    em = ec["EcuM"]
    users = em.get("EcuMFlexUserConfig", [])
    out = OrderedDict()

    def h(w):
        _define(w, "ECUM_NUM_USERS", "%du" % len(users))
        for i, u in enumerate(users):
            _define(w, "EcuMConf_" + id_name("EcuMFlexUserConfig", u["name"]), "%du" % i)
    fn, txt = _cfg_h(ecu, "EcuM", "ECU state manager configuration ids of ECU %s (flexible users that may request RUN)." % ecu.name, h)
    out[fn] = txt
    lists = [("Zero", em["EcuMDriverInitListZero"], "before the OS starts: tracing and error handling must exist first"),
             ("One", em["EcuMDriverInitListOne"], "before the OS starts: clock, pins and the ADC are needed by everything after"),
             ("Two", em["EcuMDriverInitListTwo"], "after the OS started (called from the BswM action AL_Startup, in task context)")]
    exprs = [e for _, l, _ in lists for e in l]
    w = W()
    w(banner("EcuM_Cfg.c", ecu, "EcuM callout bodies EcuM_AL_DriverInitZero/One/Two of ECU %s. In a real project the ECU integrator "
             "writes these callouts by hand or the EcuM configurator generates them from EcuMDriverInitListZero/One/Two; the "
             "ORDER of the calls is the dependency order of the drivers. Also holds Det_Config." % ecu.name, CFG_PHASE).rstrip("\n"))
    w('#include "EcuM.h"')
    for hh in headers_for(exprs):
        if hh != "EcuM.h":
            w('#include "%s"' % hh)
    w()
    w("/* Det_Config (DetDevErrorDetect = %s in %s): Det has no configurable parameters in this implementation */" % (
        ec.get("Det", {}).get("DetDevErrorDetect"), ecu.name + ".ecuc.json"))
    w("const Det_ConfigType Det_Config = { 0u };")
    w()
    for n, l, why in lists:
        w("/* EcuMDriverInitList%s: %s */" % (n, why))
        w("void EcuM_AL_DriverInit%s(void)" % n)
        w("{")
        for e in l:
            w("    (void)%s;" % e)
        w("}")
        w()
    out["EcuM_Cfg.c"] = w.text()
    return out


# ------------------------------------------------------------------------------------------------ BswM
def gen_bswm(ecu):
    ec = ecu.ecuc
    bm = ec["BswM"]
    out = OrderedDict()

    def h(w):
        _define(w, "BSWM_NUM_RULES", "%du" % len(bm["BswMRule"]))
        _define(w, "BSWM_NUM_REQUEST_USERS", "%du" % bm.get("numRequestUsers", 1))
    fn, txt = _cfg_h(ecu, "BswM", "BSW mode manager sizes of ECU %s." % ecu.name, h)
    out[fn] = txt
    exprs = [i["call"] for al in bm["BswMActionList"] for i in al["items"]]
    for r in bm["BswMRule"]:
        exprs += [str(r["arg"]), str(r["expected"])]
    w = W()
    w(banner("BswM_Cfg.c", ecu, "BswM rules and action lists of ECU %s. A BswMRule says: when the mode request <source> equals "
             "<expected> run action list <true> (else <false>). The action lists below ARE the ECU start-up and mode-change "
             "choreography: AL_Startup initialises the BSW layers above the drivers in dependency order and finally starts the RTE; "
             "AL_Run opens the communication; mode actions call the RTE's Rte_Switch API (the BswM -> RTE edge of the architecture)." % ecu.name,
             CFG_PHASE).rstrip("\n"))
    w('#include "BswM.h"')
    for hh in headers_for(exprs):
        if hh != "BswM.h":
            w('#include "%s"' % hh)
    w('#include "MemMap.h"')
    w()
    for al in bm["BswMActionList"]:
        w("/* ---- action list %s ---- */" % al["name"])
        for it in al["items"]:
            w("static Std_ReturnType BswM_Act_%s_%s(void)" % (al["name"], it["name"]))
            w("{")
            if returns_std(it["call"]):
                w("    return %s;" % it["call"])
            else:
                w("    %s;" % it["call"])
                w("    return E_OK;")
            w("}")
        w("static const BswM_ActionItemType BswM_Items_%s[] MINI_CONST_CFG = {" % al["name"])
        for k, it in enumerate(al["items"]):
            w('    { BswM_Act_%s_%s, "%s" }%s' % (al["name"], it["name"], it["name"], "," if k < len(al["items"]) - 1 else ""))
        w("};")
        w("static const BswM_ActionListType BswM_List_%s MINI_CONST_CFG = { %du, BswM_Items_%s };" % (al["name"], len(al["items"]), al["name"]))
        w()
    names = [al["name"] for al in bm["BswMActionList"]]

    def val(v):
        return ("%du" % v) if isinstance(v, int) else str(v)
    w("/* ---- rules ---- */")
    w("static const BswM_RuleType BswM_Rules[BSWM_NUM_RULES] MINI_CONST_CFG = {")
    for k, r in enumerate(bm["BswMRule"]):
        w("    { .name = \"%s\", .source = BSWM_SRC_%s, .arg = %s, .expected = %s," % (r["name"], r["source"], val(r["arg"]), val(r["expected"])))
        w("      .trueList = &BswM_List_%s, .falseList = %s }%s" % (r["true"], ("&BswM_List_%s" % r["false"]) if r.get("false") else "NULL_PTR",
                                                                  "," if k < len(bm["BswMRule"]) - 1 else ""))
    w("};")
    w()
    w("const BswM_ConfigType BswM_Config = {")
    w("    .numRules = BSWM_NUM_RULES,")
    w("    .rules = BswM_Rules,")
    w("    .numRequestUsers = BSWM_NUM_REQUEST_USERS")
    w("};")
    out["BswM_Cfg.c"] = w.text()
    return out


# ------------------------------------------------------------------------------------------------ MCAL
def gen_mcal(ecu):
    ec = ecu.ecuc
    out = OrderedDict()
    # ---- Mcu
    clk = ec["Mcu"]["McuClockSettingConfig"]

    def h(w):
        for i, c in enumerate(clk):
            _define(w, "McuConf_" + id_name("McuClockSettingConfig", c["name"]), "%du" % i)
    fn, txt = _cfg_h(ecu, "Mcu", "MCU driver clock setting ids of ECU %s." % ecu.name, h)
    out[fn] = txt
    w = W()
    w(banner("Mcu_Cfg.c", ecu, "MCU driver configuration of ECU %s: target system clock (PLL setting selected by Mcu_InitClock)." % ecu.name, CFG_PHASE).rstrip("\n"))
    w('#include "Mcu.h"')
    w('#include "MemMap.h"')
    w()
    w("const Mcu_ConfigType Mcu_Config = { .sysClockHz = %du, .clockSettings = %du };" % (clk[0]["sysClockHz"], len(clk)))
    out["Mcu_Cfg.c"] = w.text()

    # ---- Port
    pins = ec["Port"]["PortPin"]

    def h(w):
        _define(w, "PORT_NUM_PINS", "%du" % len(pins))
        w()
        w("/* PortPin id = pin number = 16 * port letter + bit (PA1 = 1, PB7 = 23, PC7 = 39) */")
        for p in pins:
            _define(w, "PortConf_" + id_name("PortPin", p["name"]), "%du" % pin_number(p["pin"]), p["pin"])
    fn, txt = _cfg_h(ecu, "Port", "Port driver pin ids of ECU %s." % ecu.name, h)
    out[fn] = txt
    w = W()
    w(banner("Port_Cfg.c", ecu, "Port driver pin configuration of ECU %s: direction, alternate-function number (mode), initial level, "
             "pull and output type of every pin that application hardware uses. Port_Init applies it once, before the OS starts." % ecu.name, CFG_PHASE).rstrip("\n"))
    w('#include "Port.h"')
    w('#include "MemMap.h"')
    w()
    w("static const Port_PinConfigType Port_Pins[PORT_NUM_PINS] MINI_CONST_CFG = {")
    for k, p in enumerate(pins):
        w("    { .pin = PortConf_%s, .direction = PORT_PIN_%s, .mode = %du, .directionChangeable = FALSE, .initialLevel = %s, .pull = PORT_PULL_%s, .openDrain = %s }%s   /* %s %s */" % (
            id_name("PortPin", p["name"]), p["direction"], p["mode"], "STD_HIGH" if p.get("level") else "STD_LOW", p["pull"],
            "TRUE" if p.get("openDrain") else "FALSE", "," if k < len(pins) - 1 else "", p["pin"], p["name"]))
    w("};")
    w()
    w("const Port_ConfigType Port_Config = { .numPins = PORT_NUM_PINS, .pins = Port_Pins };")
    out["Port_Cfg.c"] = w.text()

    # ---- Dio
    chs = ec.get("Dio", {}).get("DioChannel", [])

    def h(w):
        _define(w, "DIO_NUM_CHANNELS", "%du" % len(chs))
        if not chs:
            w("/* this ECU has no DIO channels (SensorEcu): IoHwAb_SetHeadlight/GetHeadlight return E_NOT_OK because DioConf_DioChannel_HeadlightLow is undefined */")
        for c in chs:
            _define(w, "DioConf_" + id_name("DioChannel", c["name"]), "%du" % pin_number(c["pin"]), c["pin"])
    fn, txt = _cfg_h(ecu, "Dio", "DIO channel ids of ECU %s (channel id = pin number, like AUTOSAR's port*16+bit)." % ecu.name, h)
    out[fn] = txt
    w = W()
    w(banner("Dio_Cfg.c", ecu, "DIO driver has no post-build configuration structure in this implementation (the channel ids in Dio_Cfg.h are "
             "the whole configuration); this file only records how many channels were configured, e.g. for tests.", CFG_PHASE).rstrip("\n"))
    w('#include "Dio.h"')
    w()
    w("const uint8 Dio_Cfg_NumChannels = DIO_NUM_CHANNELS;")
    out["Dio_Cfg.c"] = w.text()

    # ---- Adc
    gs = ec["Adc"]["AdcGroup"]

    def h(w):
        _define(w, "ADC_NUM_GROUPS", "%du" % len(gs))
        for i, g in enumerate(gs):
            _define(w, "AdcConf_" + id_name("AdcGroup", g["name"]), "%du" % i, "ADC1 channel %d" % g["channel"])
    fn, txt = _cfg_h(ecu, "Adc", "ADC group ids of ECU %s." % ecu.name, h)
    out[fn] = txt
    w = W()
    w(banner("Adc_Cfg.c", ecu, "ADC driver configuration of ECU %s: one conversion group per measured quantity (channel + sample time)." % ecu.name, CFG_PHASE).rstrip("\n"))
    w('#include "Adc.h"')
    w('#include "MemMap.h"')
    w()
    w("static const Adc_GroupConfigType Adc_Groups[ADC_NUM_GROUPS] MINI_CONST_CFG = {")
    for k, g in enumerate(gs):
        w("    { .channel = %du, .sampleTime = %du }%s   /* %s */" % (g["channel"], g["sampleTime"], "," if k < len(gs) - 1 else "", g["name"]))
    w("};")
    w()
    w("const Adc_ConfigType Adc_Config = { .numGroups = ADC_NUM_GROUPS, .groups = Adc_Groups };")
    out["Adc_Cfg.c"] = w.text()
    return out


def gen_all_bsw(ecu):
    out = OrderedDict()
    for g in (gen_com, gen_pdur, gen_canif, gen_can, gen_ecum, gen_bswm, gen_mcal):
        out.update(g(ecu))
    return out
