"""
emit_rte.py - emits the RTE: Rte_Type.h, Rte.h, Rte_Cbk.h, Rte_<Swc>.h, Rte_<Swc>_Type.h, Rte.c, Rte_Tasks.c.

[Educational Implementation]
Real AUTOSAR counterpart: the RTE generator.  Its three phases are visible in the output:
    contract phase    -> Rte_<Swc>.h (what a SWC may call; only configured access points exist)
    generation phase  -> Rte.c / Rte_Tasks.c (how the calls are realised for THIS ECU: Com signal, RTE buffer,
                         direct call, OS event ...)
    task body phase   -> Rte_Tasks.c (SWS_Rte_06200: the RTE generator constructs the OS task bodies)
The generated C is deliberately verbose and commented: it is a teaching artefact.
"""
from collections import OrderedDict

from emit_util import banner, W, guard, comment_box, RTE_PHASE
from arxml_model import c_hex

BSW_HEADERS = OrderedDict([("Can_", "Can.h"), ("CanIf_", "CanIf.h"), ("Com_", "Com.h"), ("PduR_", "PduR.h"), ("EcuM_", "EcuM.h"),
                           ("BswM_", "BswM.h"), ("Det_", "Det.h"), ("SchM_", "SchM.h"), ("IoHwAb_", "IoHwAb.h"),
                           ("Mcu_", "Mcu.h"), ("Port_", "Port.h"), ("Dio_", "Dio.h"), ("Adc_", "Adc.h"), ("Trace_", "Trace.h"),
                           ("Rte_", "Rte.h")])


def header_for(func):
    for pre, h in BSW_HEADERS.items():
        if func.startswith(pre):
            return h
    return None


def swc_files(ecu):
    """application SWCs with their file names (Rte_<SwcType>.h)"""
    return [(p, "Rte_%s.h" % p.type.name) for p in ecu.apps]


def arg_decl(a):
    return ("%s *%s" % (a.type, a.name)) if a.dir in ("OUT", "INOUT") else ("%s %s" % (a.type, a.name))


def op_params(op):
    return ", ".join(arg_decl(a) for a in op.args) or "void"


def op_args(op):
    return ", ".join(a.name for a in op.args)


def mode_c_name(group, mode):
    return "RTE_MODE_%s_%s" % (group.name, mode)


def trace_enabled(ecu, key):
    return bool(ecu.ecuc["Rte"].get("RteTrace", {}).get(key, False))


def wake_code(ecu, mp):
    """C statement(s) that make the task of mapping `mp` run for this event (RTE -> OS interaction)."""
    t = ecu.tasks[mp.task]
    osev = (mp.trigger or {}).get("osEvent")
    if t.type == "EXTENDED":
        return "(void)SetEvent(%s, %s);" % (t.name, osev), "SetEvent"
    return "(void)ActivateTask(%s);" % t.name, "ActivateTask"


def event_desc(ev):
    if ev.kind == "TIMING":
        return "TimingEvent %d ms" % ev.period_ms
    if ev.kind == "DATA_RECEIVED":
        return "DataReceivedEvent %s.%s" % ev.data
    if ev.kind == "MODE_SWITCH":
        return "ModeSwitchEvent %s %s %s" % (ev.activation, ev.mode[0], ev.mode[1])
    return "OperationInvokedEvent %s.%s" % ev.op


# ================================================================================================ Rte_Type.h
def gen_rte_type_h(ecu):
    w = W()
    fn = "Rte_Type.h"
    w(banner(fn, ecu, "Data types of the RTE: implementation data types and mode declaration groups. Every Rte_<Swc>.h includes "
             "this file (in a real RTE: SWS_Rte_01007 types come from Rte_Type.h).", RTE_PHASE).rstrip("\n"))
    w("#ifndef %s" % guard(fn))
    w("#define %s" % guard(fn))
    w()
    w('#include "Rte_Common.h"   /* hand-written, fixed part: RTE_E_* codes, Rte_Start/Rte_Stop */')
    w()
    w("/* ---- implementation data types (SwcTypes.arxml IMPLEMENTATION-DATA-TYPE) ---- */")
    for n, b in ecu.swc.dtypes.items():
        if n == b:
            w("/* %-8s = Platform_Types.h %s (no typedef needed) */" % (n, b))
        else:
            w("typedef %s %s;" % (b, n))
    w()
    for g in ecu.swc.modegroups.values():
        w("/* ---- mode declaration group %s (initial mode %s) ---- */" % (g.name, g.initial))
        w("typedef uint8 Rte_ModeType_%s;" % g.name)
        for mn, mv in g.modes.items():
            w("#define %-34s ((Rte_ModeType_%s)%du)" % (mode_c_name(g, mn), g.name, mv))
        w()
        if ecu.mode_mgr is not None and ecu.mode_mgr.group.name == g.name:
            w("/* The RTE-owned mode machine variable: written only by Rte_Switch_<p>_<m>() in Rte.c (under OS interrupt lock);")
            w(" * read atomically (one byte) by Rte_Mode_<p>_<m>() in the SWC contract headers and by the task bodies for")
            w(" * disabledInMode checks. 'volatile' because tasks and the mode manager (BswM) are different execution contexts. */")
            w("extern volatile Rte_ModeType_%s Rte_Mode_%s;" % (g.name, g.name))
            w()
    w("#endif /* %s */" % guard(fn))
    return w.text()


# ================================================================================================ Rte.h / Rte_Cbk.h
def gen_rte_h(ecu):
    w = W()
    fn = "Rte.h"
    w(banner(fn, ecu, "RTE interface towards the rest of the ECU software that is NOT a SWC: the mode-manager side of the mode "
             "interface (called by BswM actions, see BswM_Cfg.c) and the implicit-communication hooks used by the generated task "
             "bodies in Rte_Tasks.c. SWCs never include this file (they include only their own Rte_<Swc>.h).", RTE_PHASE).rstrip("\n"))
    w("#ifndef %s" % guard(fn))
    w("#define %s" % guard(fn))
    w()
    w('#include "Rte_Type.h"')
    w()
    if ecu.mode_mgr is not None:
        mm = ecu.mode_mgr
        w("/* Mode manager port %s.%s: this is the real 'Rte_Switch_<p>_<m>' API (SWS_Rte_02631). The mode manager is BswM:" % (mm.proto, mm.port))
        w(" * a BswM action (BswM_Cfg.c) calls it when its rule fires. Updates Rte_Mode_%s, then raises the ModeSwitchEvents." % mm.group.name)
        w(" * Returns RTE_E_OK, RTE_E_INVALID for an unknown mode. */")
        w("Std_ReturnType Rte_Switch_%s_%s(Rte_ModeType_%s mode);" % (mm.port, mm.group.name, mm.group.name))
        w()
    w("/* RTE-internal (used by Rte_Tasks.c only): implicit communication. CopyIn takes the snapshot of the data a runnable reads")
    w(" * implicitly (Rte_IRead), CopyOut publishes what it wrote implicitly (Rte_IWrite) - see SWS 4.3.1.5.1 'implicit")
    w(" * communication behaviour': read at runnable start, write at runnable termination. */")
    for (pn, rn), ri in ecu.runnable_info.items():
        if ri.ird:
            w("void Rte_CopyIn_%s(void);" % rn)
        if ri.iwr:
            w("void Rte_CopyOut_%s(void);" % rn)
    w()
    w("#endif /* %s */" % guard(fn))
    return w.text()


def gen_rte_cbk_h(ecu):
    w = W()
    fn = "Rte_Cbk.h"
    w(banner(fn, ecu, "Callback prototypes of the RTE towards the BSW (SWS: Rte_Cbk.h, 'Rte_COMCbk_<signal>'). Referenced by the "
             "ComSignal notification pointers in Com_Cfg.c.", RTE_PHASE).rstrip("\n"))
    w("#ifndef %s" % guard(fn))
    w("#define %s" % guard(fn))
    w()
    w('#include "Std_Types.h"')
    w()
    seen = []
    for mp in ecu.maps:
        if mp.trigger and mp.trigger.get("type") == "COM_NOTIFICATION" and mp.trigger["comSignal"] not in seen:
            seen.append(mp.trigger["comSignal"])
    for s in seen:
        w("void Rte_COMCbk_%s(void);   /* called by Com after reception of signal %s (DataReceivedEvent trigger) */" % (s, s))
    if not seen:
        w("/* no Com notifications are mapped to RTE events on this ECU */")
    w()
    w("#endif /* %s */" % guard(fn))
    return w.text()


# ================================================================================================ Rte_<Swc>_Type.h / Rte_<Swc>.h
def gen_swc_type_h(ecu, p):
    fn = "Rte_%s_Type.h" % p.type.name
    w = W()
    w(banner(fn, ecu, "Per-instance-memory (PIM) types of SWC %s. A PIM is private static state of one SWC instance, allocated "
             "and initialised by the RTE (instead of file-scope statics in the SWC) so that the RTE knows the memory (partitioning, "
             "MemMap sections) and the SWC stays instantiable several times." % p.type.name, RTE_PHASE).rstrip("\n"))
    w("#ifndef %s" % guard(fn))
    w("#define %s" % guard(fn))
    w()
    w('#include "Rte_Type.h"')
    w()
    for pim in p.type.pims:
        w("/* %s: ARXML PER-INSTANCE-MEMORY '%s'; accessed with Rte_Pim_%s() */" % (pim.type, pim.name, pim.name))
        w("typedef struct {")
        for (fname, ftype) in pim.fields:
            w("    %s %s;" % (ftype, fname))
        w("} %s;" % pim.type)
        w()
    w("#endif /* %s */" % guard(fn))
    return fn, w.text()


def _swc_accesses(ecu, p):
    """deduplicated (per SWC) lists of what the contract header must declare."""
    rd, wr, calls, modes, ird, iwr = OrderedDict(), OrderedDict(), OrderedDict(), OrderedDict(), [], []
    for (pn, rname), ri in ecu.runnable_info.items():
        if pn != p.name:
            continue
        for (port, elem, src) in ri.rd:
            rd.setdefault((port, elem), src)
        for (port, elem, sk) in ri.wr:
            wr.setdefault((port, elem), sk)
        for (port, op, srv) in ri.calls:
            calls.setdefault((port, op.name), (op, srv))
        for (port, grp) in ri.modes:
            modes.setdefault((port, grp), True)
        for (port, elem, src) in ri.ird:
            ird.append((rname, port, elem, src))
        for (port, elem, sk) in ri.iwr:
            iwr.append((rname, port, elem, sk))
    return rd, wr, calls, modes, ird, iwr


def runnable_proto(ecu, p, rn):
    ri = ecu.runnable_info[(p.name, rn.name)]
    if ri.is_server:
        ev = [e for e in p.type.events if e.kind == "OPERATION_INVOKED" and e.runnable == rn.name][0]
        op = ecu.swc.ifaces[p.type.ports[ev.op[0]].iface].ops[ev.op[1]]
        return "Std_ReturnType %s(%s)" % (rn.symbol, op_params(op))
    return "void %s(void)" % rn.symbol


def gen_swc_h(ecu, p):
    fn = "Rte_%s.h" % p.type.name
    st = p.type
    rd, wr, calls, modes, ird, iwr = _swc_accesses(ecu, p)
    w = W()
    w(banner(fn, ecu, "APPLICATION HEADER (contract) of SWC type %s: the ONLY header its C file includes. It declares exactly the "
             "RTE API this SWC is configured for - one function/macro per access point in the ARXML. Calling anything else is a "
             "compile error, which is how AUTOSAR enforces that a SWC only uses its declared ports (SWS_Rte_01004, SWS_Rte_02751).\n"
             "Naming: Rte_Read/Write_<port>_<element>, Rte_IRead/IWrite_<runnable>_<port>_<element>, "
             "Rte_Call_<port>_<operation>, Rte_Mode_<port>_<modegroup>, Rte_Pim_<name>, Rte_Enter/Exit_<exclusiveArea>." % st.name,
             RTE_PHASE).rstrip("\n"))
    w("#ifndef %s" % guard(fn))
    w("#define %s" % guard(fn))
    w()
    w('#include "Rte_Type.h"')
    if st.pims:
        w('#include "Rte_%s_Type.h"' % st.name)
    w()

    # --- runnables
    w("/* ================================================================== runnables this SWC must implement")
    w(" * The RTE calls them; the SWC never does. Where each one runs is decided by RteEventToTaskMapping in %s. */" % ecu.ecuc_file)
    for rn in st.runnables.values():
        ev = [e for e in st.events if e.runnable == rn.name]
        w("/* %s: %s */" % (rn.name, "; ".join("%s '%s'" % (event_desc(e), e.name) for e in ev)))
        mps = [m for m in ecu.maps if m.proto == p.name and m.runnable.name == rn.name]
        tasks = []
        for m in mps:
            tasks.append("%s (RtePositionInTask %s)" % (m.task, m.position) if m.task else "no task: direct call in the caller's context")
        w("/*   executed in: %s */" % "; ".join(OrderedDict((t, 1) for t in tasks)))
        w("%s;" % runnable_proto(ecu, p, rn))
    w()

    # --- explicit S/R
    if rd or wr:
        w("/* ================================================================== explicit sender/receiver (SWS_Rte_01091 / SWS_Rte_01071)")
        w(" * The call itself is the communication point: Rte_Write sends NOW, Rte_Read fetches the latest value NOW. */")
        for (port, elem), src in rd.items():
            origin = ("Com signal %s" % src.signal) if src.kind == "COM" else "RTE-internal buffer %s" % src.buf
            w("/* read  %s.%s: data comes from %s. Returns RTE_E_OK, RTE_E_COM_STOPPED before Rte_Start / Com stopped */" % (port, elem, origin))
            w("Std_ReturnType Rte_Read_%s_%s(%s *data);" % (port, elem, src.type))
        for (port, elem), sk in wr.items():
            dest = []
            if sk.signal:
                dest.append("Com signal %s" % sk.signal)
            if sk.buf:
                dest.append("RTE-internal buffer (+ activation of the receiving task)")
            w("/* write %s.%s: data goes to %s. Returns RTE_E_OK or RTE_E_COM_STOPPED */" % (port, elem, " and ".join(dest)))
            w("Std_ReturnType Rte_Write_%s_%s(%s data);" % (port, elem, sk.type))
        w()

    # --- implicit S/R
    if ird or iwr:
        w("/* ================================================================== implicit sender/receiver (SWS_Rte_03741 / SWS_Rte_03744)")
        w(" * Per-runnable buffers: the RTE copies data IN before the runnable starts (Rte_CopyIn_<runnable>) and OUT after it")
        w(" * terminated (Rte_CopyOut_<runnable>), both from the task body in Rte_Tasks.c. The runnable therefore sees a stable")
        w(" * snapshot and needs no Exclusive Area; the buffers are exposed here as extern variables so that IRead/IWrite are")
        w(" * plain macros (zero call overhead) - the usual 'optimised RTE' shape. */")
        for (rname, port, elem, src) in ird:
            w("extern %s Rte_Irb_%s_%s_%s;" % (src.type, rname, port, elem))
            w("#define Rte_IRead_%s_%s_%s()  (Rte_Irb_%s_%s_%s)" % (rname, port, elem, rname, port, elem))
        for (rname, port, elem, sk) in iwr:
            w("extern %s Rte_Iwb_%s_%s_%s;" % (sk.type, rname, port, elem))
            w("#define Rte_IWrite_%s_%s_%s(data)  ((void)(Rte_Iwb_%s_%s_%s = (data)))" % (rname, port, elem, rname, port, elem))
        w()

    # --- C/S
    if calls:
        w("/* ================================================================== client/server (SWS_Rte_01102 Rte_Call)")
        w(" * Synchronous call; returns RTE_E_OK (0) or the server's application error. IN args by value, OUT args by pointer. */")
        for (port, opn), (op, srv) in calls.items():
            if srv.kind == "RUNNABLE":
                tgt = "server runnable %s() of SWC %s (same ECU: direct call in the CALLER's task context)" % (srv.runnable.symbol, srv.proto)
            else:
                tgt = "BSW function %s() (RteBswServerMapping)" % srv.function
            w("/* call %s.%s -> %s */" % (port, opn, tgt))
            w("Std_ReturnType Rte_Call_%s_%s(%s);" % (port, opn, op_params(op)))
        w()

    # --- mode
    if modes:
        w("/* ================================================================== mode (SWS_Rte_02628 Rte_Mode) */")
        for (port, grp), _ in modes.items():
            w("/* current mode of %s.%s (read-only for a mode user; the mode manager switches it) */" % (port, grp))
            w("#define Rte_Mode_%s_%s()  ((Rte_ModeType_%s)Rte_Mode_%s)" % (port, grp, grp, grp))
        w()

    # --- PIM
    if st.pims:
        w("/* ================================================================== per-instance memory (Rte_Pim) */")
        for pim in st.pims:
            w("extern %s Rte_Pim_%s_%s;   /* storage lives in Rte.c (section .bss.rte), initialised by Rte_Start() */" % (pim.type, p.name, pim.name))
            w("#define Rte_Pim_%s()  (&Rte_Pim_%s_%s)" % (pim.name, p.name, pim.name))
        w()

    # --- exclusive areas
    ea_names = []
    for rn in st.runnables.values():
        for a in rn.eas:
            if a not in ea_names:
                ea_names.append(a)
    if ea_names:
        w("/* ================================================================== exclusive areas (SWS_Rte_01120 Rte_Enter / Rte_Exit) */")
        for a in ea_names:
            impl = ecu.eas[a]["implementation"]
            w("/* %s: implemented as %s%s */" % (a, impl, (" (OS resource %s, priority ceiling %d)" % (ecu.eas[a]["resource"], ecu.ceilings[ecu.eas[a]["resource"]])
                                                    if impl == "OS_RESOURCE" else "")))
            w("void Rte_Enter_%s(void);" % a)
            w("void Rte_Exit_%s(void);" % a)
        w()
    w("#endif /* %s */" % guard(fn))
    return fn, w.text()


# ================================================================================================ Rte.c
def _init_literal(v):
    return "%du" % v if v >= 0 else "(%d)" % v


def gen_rte_c(ecu):
    w = W()
    fn = "Rte.c"
    tr = lambda k: trace_enabled(ecu, k)
    have_com = bool(ecu.csigs)
    w(banner(fn, ecu, "The RTE of ECU %s: communication buffers, the realisation of every Rte_ API for this ECU's connections "
             "(Com signal, RTE-internal buffer, direct server call, OS event), mode switching, exclusive areas, the Com "
             "notification callbacks that trigger DataReceivedEvents, and Rte_Start/Rte_Stop. Where a SWC port is connected to is "
             "decided by System.arxml (assembly connectors + S/R-to-signal mapping) and the ECUC (Com signals); this file is the "
             "result of that decision." % ecu.name, RTE_PHASE, notes=["Spec: SWS_Rte_01071 Rte_Write, 01091 Rte_Read, 02631 Rte_Switch, 01120 Rte_Enter, 03741/03744 Rte_IRead/IWrite."]).rstrip("\n"))
    w('#include "Rte.h"')
    w('#include "Rte_Cbk.h"')
    w('#include "Os.h"           /* the RTE is the only user of OS services on behalf of SWCs (Activate/SetEvent/Resources) */')
    if have_com:
        w('#include "Com.h"          /* Com_SendSignal / Com_ReceiveSignal: inter-ECU S/R is realised by Com */')
    bsw_h = []
    for k, m in ecu.bsw_servers.items():
        if m.get("header") and m["header"] not in bsw_h:
            bsw_h.append(m["header"])
    for h in bsw_h:
        w('#include "%s"        /* BSW server of a client/server port (RteBswServerMapping) */' % h)
    w('#include "Trace.h"')
    w('#include "MemMap.h"')
    w("/* the contract headers are included here too: the compiler then checks that the definitions below match the declarations the SWCs see */")
    for p, h in swc_files(ecu):
        w('#include "%s"' % h)
    w()

    w(comment_box("RTE state", [
        "All RTE variables are zero-initialised (.bss.rte) and set to their init values by Rte_Start(): the data of a",
        "SWC must not become valid before the BSW (Com, ...) was initialised, which BswM does in AL_Startup."]))
    w("MINI_VAR_RTE_BUF static boolean Rte_Started;")
    if ecu.mode_mgr is not None:
        g = ecu.mode_mgr.group
        w("MINI_VAR_RTE_BUF volatile Rte_ModeType_%s Rte_Mode_%s;   /* declared in Rte_Type.h */" % (g.name, g.name))
    w()

    # ---- buffers
    bufs = [sk for sk in ecu.sinks.values() if sk.buf]
    if bufs:
        w(comment_box("RTE-internal communication buffers (one per provider data element that has an on-ECU receiver)", [
            "A write stores here (under OS interrupt lock - the buffer may be read from another task), a read copies out."]))
        for sk in bufs:
            w("MINI_VAR_RTE_BUF static %s %s;   /* %s.%s.%s */" % (sk.type, sk.buf, sk.proto, sk.port, sk.elem))
        w()
    ibufs = []
    for (pn, rname), ri in ecu.runnable_info.items():
        for (port, elem, src) in ri.ird:
            ibufs.append(("Irb", rname, port, elem, src.type, src.init))
        for (port, elem, sk) in ri.iwr:
            ibufs.append(("Iwb", rname, port, elem, sk.type, sk.init))
    if ibufs:
        w(comment_box("Implicit-communication buffers (one per runnable access point, declared extern in Rte_<Swc>.h)", [
            "Irb = implicit read buffer (filled by Rte_CopyIn_<runnable> before the runnable runs),",
            "Iwb = implicit write buffer (published by Rte_CopyOut_<runnable> after it ran). Persistent between activations,",
            "so a runnable that does not call Rte_IWrite re-publishes its last value (last-is-best)."]))
        for (kind, rname, port, elem, ty, init) in ibufs:
            w("MINI_VAR_RTE_BUF %s Rte_%s_%s_%s_%s;" % (ty, kind, rname, port, elem))
        w()
    pims = [(p, pim) for p in ecu.apps for pim in p.type.pims]
    if pims:
        w(comment_box("Per-instance memory (PIM) storage + init values (ARXML PER-INSTANCE-MEMORY / INIT-VALUE)", []))
        for p, pim in pims:
            w("MINI_VAR_RTE_BUF %s Rte_Pim_%s_%s;" % (pim.type, p.name, pim.name))
            w("MINI_CONST_CFG static const %s Rte_PimInit_%s_%s = %s;" % (pim.type, p.name, pim.name, pim.init))
        w()

    # ---- helpers
    if have_com:
        w("/* Com returns E_OK / COM_SERVICE_NOT_AVAILABLE (I-PDU group stopped) / COM_BUSY; the RTE API speaks RTE_E_*. */")
        w("static Std_ReturnType Rte_MapComStatus(uint8 comResult)")
        w("{")
        w("    Std_ReturnType rc;")
        w("    if (comResult == E_OK)")
        w("    {")
        w("        rc = RTE_E_OK;")
        w("    }")
        w("    else if (comResult == COM_SERVICE_NOT_AVAILABLE)")
        w("    {")
        w("        rc = RTE_E_COM_STOPPED;")
        w("    }")
        w("    else")
        w("    {")
        w("        rc = RTE_E_LIMIT;")
        w("    }")
        w("    return rc;")
        w("}")
        w()

    # ---- sinks (the 'send side' of S/R, shared by Rte_Write and Rte_CopyOut)
    sinks = [sk for sk in ecu.sinks.values() if sk.written]
    if sinks:
        w(comment_box("Send side of sender/receiver: one internal 'sink' per written provider data element", [
            "Rte_Write_<p>_<e>() (explicit) and Rte_CopyOut_<runnable>() (implicit) both end up here, so there is exactly one",
            "place that knows where the data goes: a Com signal (inter-ECU), an RTE buffer (intra-ECU), or both."]))
        for sk in sinks:
            name = "Rte_Sink_%s_%s_%s" % (sk.proto, sk.port, sk.elem)
            w("static Std_ReturnType %s(%s data)" % (name, sk.type))
            w("{")
            w("    Std_ReturnType rc = RTE_E_OK;")
            w()
            w("    if (Rte_Started == FALSE)")
            w("    {")
            w("        return RTE_E_COM_STOPPED;")
            w("    }")
            if tr("writes"):
                w('    TRACE(TRACE_CAT_RTE, "WRITE %s_%s=%%u", (unsigned)data);' % (sk.port, sk.elem))
            if sk.buf:
                w("    SuspendOSInterrupts();            /* intra-ECU: another task may read the buffer concurrently */")
                w("    %s = data;" % sk.buf)
                w("    ResumeOSInterrupts();")
            if sk.signal:
                w("    rc = Rte_MapComStatus(Com_SendSignal(ComConf_ComSignal_%s, &data));   /* inter-ECU: Com packs it into I-PDU %s */" % (
                    sk.signal, ecu.csigs[sk.signal]["ipdu"]))
            for mp in sk.triggers:
                code, kind = wake_code(ecu, mp)
                w("    /* DataReceivedEvent %s (RteEventToTaskMapping, trigger INTERNAL_WRITE): wake %s via %s */" % (mp.event.name, mp.task, kind))
                w('    TRACE(TRACE_CAT_RTE, "TRIGGER %s -> %s");' % (mp.event.name, mp.task))
                w("    %s" % code)
                w("    /* an E_OS_LIMIT here means the task is still pending: it will pick up the newest value anyway (last-is-best) */")
            w("    return rc;")
            w("}")
            w()

    # ---- explicit S/R API
    any_api = False
    for p in ecu.apps:
        rd, wr, calls, modes, ird, iwr = _swc_accesses(ecu, p)
        if rd or wr:
            if not any_api:
                w(comment_box("Explicit sender/receiver API (Rte_Read / Rte_Write)", []))
                any_api = True
        for (port, elem), src in rd.items():
            w("/* %s: reads %s.%s from %s */" % (p.name, port, elem, ("Com signal " + src.signal) if src.kind == "COM" else "buffer " + src.buf))
            w("Std_ReturnType Rte_Read_%s_%s(%s *data)" % (port, elem, src.type))
            w("{")
            w("    Std_ReturnType rc;")
            w()
            w("    if (Rte_Started == FALSE)")
            w("    {")
            w("        return RTE_E_COM_STOPPED;")
            w("    }")
            if src.kind == "COM":
                w("    rc = Rte_MapComStatus(Com_ReceiveSignal(ComConf_ComSignal_%s, data));" % src.signal)
            else:
                w("    SuspendOSInterrupts();")
                w("    *data = %s;" % src.buf)
                w("    ResumeOSInterrupts();")
                w("    rc = RTE_E_OK;")
            if tr("reads"):
                w("    if (rc == RTE_E_OK)")
                w("    {")
                w('        TRACE(TRACE_CAT_RTE, "READ %s_%s=%%u", (unsigned)*data);' % (port, elem))
                w("    }")
            w("    return rc;")
            w("}")
            w()
        for (port, elem), sk in wr.items():
            w("/* %s: writes %s.%s */" % (p.name, port, elem))
            w("Std_ReturnType Rte_Write_%s_%s(%s data)" % (port, elem, sk.type))
            w("{")
            w("    return Rte_Sink_%s_%s_%s(data);" % (sk.proto, sk.port, sk.elem))
            w("}")
            w()

    # ---- implicit copy in/out
    first = True
    for (pn, rname), ri in ecu.runnable_info.items():
        if ri.ird:
            if first:
                w(comment_box("Implicit communication: copy-in / copy-out around runnables (called from Rte_Tasks.c)", []))
                first = False
            w("void Rte_CopyIn_%s(void)" % rname)
            w("{")
            for (port, elem, src) in ri.ird:
                v = "Rte_Irb_%s_%s_%s" % (rname, port, elem)
                if src.kind == "COM":
                    w("    /* snapshot of Com signal %s; if Com has nothing new the previous snapshot stays */" % src.signal)
                    w("    (void)Com_ReceiveSignal(ComConf_ComSignal_%s, &%s);" % (src.signal, v))
                else:
                    w("    SuspendOSInterrupts();")
                    w("    %s = %s;" % (v, src.buf))
                    w("    ResumeOSInterrupts();")
            w("}")
            w()
        if ri.iwr:
            if first:
                w(comment_box("Implicit communication: copy-in / copy-out around runnables (called from Rte_Tasks.c)", []))
                first = False
            w("void Rte_CopyOut_%s(void)" % rname)
            w("{")
            for (port, elem, sk) in ri.iwr:
                w("    (void)Rte_Sink_%s_%s_%s(Rte_Iwb_%s_%s_%s);" % (sk.proto, sk.port, sk.elem, rname, port, elem))
            w("}")
            w()

    # ---- C/S
    first = True
    for p in ecu.apps:
        rd, wr, calls, modes, ird, iwr = _swc_accesses(ecu, p)
        for (port, opn), (op, srv) in calls.items():
            if first:
                w(comment_box("Client/server API (Rte_Call)", [
                    "Same-ECU server runnable: the RTE calls it directly, i.e. it executes in the CALLER's task with the caller's priority",
                    "(that is why the server's exclusive area must consider every client task). BSW server: call the configured function.",
                    "A real RTE usually turns this into a macro; here it is a function so that the optional trace line has a home."]))
                first = False
            w("Std_ReturnType Rte_Call_%s_%s(%s)" % (port, opn, op_params(op)))
            w("{")
            if tr("calls"):
                w('    TRACE(TRACE_CAT_RTE, "CALL %s_%s");' % (port, opn))
            fn_ = srv.runnable.symbol if srv.kind == "RUNNABLE" else srv.function
            w("    return %s(%s);" % (fn_, op_args(op)))
            w("}")
            w()

    # ---- exclusive areas
    if ecu.eas:
        w(comment_box("Exclusive areas (Rte_Enter / Rte_Exit)", [
            "ECUC RteExclusiveAreaImplementation chooses the mechanism: OS_RESOURCE = GetResource/ReleaseResource (priority ceiling:",
            "tasks up to the ceiling cannot preempt the holder, nothing else is blocked); OS_INTERRUPT_BLOCKING / ALL_INTERRUPT_BLOCKING",
            "= Suspend*Interrupts (blocks ISRs too; only for very short sections)."]))
        for n, ea in ecu.eas.items():
            impl = ea["implementation"]
            w("void Rte_Enter_%s(void)" % n)
            w("{")
            if impl == "OS_RESOURCE":
                w("    (void)GetResource(%s);   /* ceiling priority %d = highest priority among tasks that can reach this area */" % (ea["resource"], ecu.ceilings[ea["resource"]]))
            elif impl == "OS_INTERRUPT_BLOCKING":
                w("    SuspendOSInterrupts();")
            else:
                w("    SuspendAllInterrupts();")
            w("}")
            w()
            w("void Rte_Exit_%s(void)" % n)
            w("{")
            if impl == "OS_RESOURCE":
                w("    (void)ReleaseResource(%s);" % ea["resource"])
            elif impl == "OS_INTERRUPT_BLOCKING":
                w("    ResumeOSInterrupts();")
            else:
                w("    ResumeAllInterrupts();")
            w("}")
            w()

    # ---- mode
    if ecu.mode_mgr is not None:
        mm = ecu.mode_mgr
        g = mm.group
        w(comment_box("Mode handling: Rte_Switch_%s_%s (mode manager side)" % (mm.port, g.name), [
            "The mode manager (BswM) requests a mode; the RTE stores it and raises every ModeSwitchEvent whose activation ('ON-ENTRY')",
            "matches the NEW mode. A SWC only observes the mode through Rte_Mode_<p>_<m>() and through its ModeSwitchEvents."]))
        w("static const char *Rte_ModeName_%s(Rte_ModeType_%s mode)" % (g.name, g.name))
        w("{")
        w("    const char *n = \"?\";")
        for mn in g.modes:
            w("    if (mode == %s) { n = \"%s\"; }" % (mode_c_name(g, mn), mn))
        w("    return n;")
        w("}")
        w()
        w("Std_ReturnType Rte_Switch_%s_%s(Rte_ModeType_%s mode)" % (mm.port, g.name, g.name))
        w("{")
        w("    Rte_ModeType_%s previous;" % g.name)
        w()
        w("    if (mode > %s)" % mode_c_name(g, max(g.modes, key=lambda k: g.modes[k])))
        w("    {")
        w("        return RTE_E_INVALID;")
        w("    }")
        w("    SuspendOSInterrupts();            /* tasks read Rte_Mode_%s without a lock: update atomically w.r.t. OS ISRs */" % g.name)
        w("    previous = Rte_Mode_%s;" % g.name)
        w("    Rte_Mode_%s = mode;" % g.name)
        w("    ResumeOSInterrupts();")
        if tr("mode"):
            w('    TRACE(TRACE_CAT_RTE, "MODE %s=%%s", Rte_ModeName_%s(mode));' % (g.name, g.name))
        w("    if (mode != previous)")
        w("    {")
        first = True
        for mn in g.modes:
            mps = [mp for mp in ecu.maps if mp.event.kind == "MODE_SWITCH" and mp.event.mode[1] == mn and mp.event.activation == "ON-ENTRY"]
            if not mps:
                continue
            w("        %sif (mode == %s)" % ("" if first else "else ", mode_c_name(g, mn)))
            w("        {")
            for mp in mps:
                code, kind = wake_code(ecu, mp)
                w("            /* ModeSwitchEvent %s: on entry of %s -> %s via %s */" % (mp.event.name, mn, mp.task, kind))
                w('            TRACE(TRACE_CAT_RTE, "TRIGGER %s -> %s");' % (mp.event.name, mp.task))
                w("            %s" % code)
            w("        }")
            first = False
        if first:
            w("        /* no ModeSwitchEvents are mapped on this ECU */")
        w("    }")
        w("    return RTE_E_OK;")
        w("}")
        w()

    # ---- Com callbacks
    cbs = OrderedDict()
    for mp in ecu.maps:
        if mp.trigger and mp.trigger.get("type") == "COM_NOTIFICATION":
            cbs.setdefault(mp.trigger["comSignal"], []).append(mp)
    if cbs:
        w(comment_box("Com notification callbacks = the trigger of DataReceivedEvents that come from the network", [
            "Com calls Rte_COMCbk_<signal>() after it unpacked a received signal (here from Com_MainFunctionRx, task context, because the",
            "I-PDU is DEFERRED; with IMMEDIATE it would be the CAN RX ISR). The RTE turns that into an OS action: SetEvent on the",
            "extended task that hosts the runnable, or ActivateTask for a basic task. The runnable itself runs later in that task."]))
        for s, mps in cbs.items():
            w("void Rte_COMCbk_%s(void)" % s)
            w("{")
            w("    if (Rte_Started == FALSE)")
            w("    {")
            w("        return;                   /* RTE not started: ignore, Rte_Start() establishes a clean state */")
            w("    }")
            for mp in mps:
                code, kind = wake_code(ecu, mp)
                w("    /* %s (%s) -> task %s via %s */" % (mp.event.name, event_desc(mp.event), mp.task, kind))
                w('    TRACE(TRACE_CAT_RTE, "TRIGGER %s -> %s");' % (mp.event.name, mp.task))
                w("    %s" % code)
            w("}")
            w()

    # ---- start/stop
    w(comment_box("Rte_Start / Rte_Stop (SWS 5.7: called by the mode manager, here BswM action AL_Startup after Com_Init)", []))
    w("Std_ReturnType Rte_Start(void)")
    w("{")
    w("    /* 1. RTE-internal buffers and implicit buffers -> init values from the ARXML (INIT-VALUE) */")
    for sk in bufs:
        w("    %s = %s;" % (sk.buf, _init_literal(sk.init)))
    for (kind, rname, port, elem, ty, init) in ibufs:
        w("    Rte_%s_%s_%s_%s = %s;" % (kind, rname, port, elem, _init_literal(init)))
    for p, pim in pims:
        w("    Rte_Pim_%s_%s = Rte_PimInit_%s_%s;" % (p.name, pim.name, p.name, pim.name))
    if ecu.mode_mgr is not None:
        g = ecu.mode_mgr.group
        w("    /* 2. mode machine starts in the initial mode of the mode declaration group */")
        w("    Rte_Mode_%s = %s;" % (g.name, mode_c_name(g, ecu.mode_mgr.initial)))
    w("    /* 3. from now on the API works and Com callbacks are honoured */")
    w("    Rte_Started = TRUE;")
    w('    TRACE(TRACE_CAT_RTE, "START");')
    w("    return RTE_E_OK;")
    w("}")
    w()
    w("Std_ReturnType Rte_Stop(void)")
    w("{")
    w("    Rte_Started = FALSE;")
    w('    TRACE(TRACE_CAT_RTE, "STOP");')
    w("    return RTE_E_OK;")
    w("}")
    return w.text()


# ================================================================================================ Rte_Tasks.c
def _guard_expr(ecu, ent):
    """disabledInMode condition for a timing runnable (None if always enabled)."""
    if not ent.disabled or not ecu.mode_mgr:
        return None
    g = ecu.mode_mgr.group
    return " && ".join("Rte_Mode_%s != %s" % (g.name, mode_c_name(g, m)) for m in ent.disabled)


def _call_runnable(w, ecu, ent, ind):
    ri = ent.info
    if ri.ird:
        w("%sRte_CopyIn_%s();      /* implicit read: snapshot before the runnable starts */" % (ind, ent.runnable.name))
    w("%s%s();" % (ind, ent.runnable.symbol))
    if ri.iwr:
        w("%sRte_CopyOut_%s();     /* implicit write: publish after the runnable terminated */" % (ind, ent.runnable.name))


def gen_rte_tasks_c(ecu):
    w = W()
    fn = "Rte_Tasks.c"
    w(banner(fn, ecu, "OS task and ISR bodies of ECU %s (SWS_Rte_06200: the RTE generator constructs the task bodies for tasks that "
             "contain runnables; SWS_Rte_06201 for tasks that contain BSW schedulable entities; SWS_Rte_04560 for category 2 ISRs). "
             "A task is just a container: RteEventToTaskMapping (ECUC_Rte_09020) says which runnable runs in which task, "
             "RtePositionInTask (ECUC_Rte_09023) in which order, and the OsAlarm that raises the event gives the activation "
             "period/offset (RteActivationOffset ECUC_Rte_09018 = alarm start). The kernel's Os_Cfg.c references these functions "
             "as task entry points." % ecu.name, RTE_PHASE).rstrip("\n"))
    w('#include "Os.h"')
    w('#include "Rte.h"')
    w('#include "Trace.h"')
    hdrs = []
    for t in ecu.tasks.values():
        for f in t.bsw:
            h = header_for(f)
            if h and h not in hdrs:
                hdrs.append(h)
    for i in ecu.ecuc["Os"].get("OsIsr", []):
        for f in i["calls"]:
            h = header_for(f)
            if h and h not in hdrs:
                hdrs.append(h)
    for h in sorted(hdrs):
        w('#include "%s"' % h)
    w("/* runnable prototypes: contract headers of the SWCs hosted here (this TU is part of the RTE, not a SWC, so it may include several) */")
    for p, h in swc_files(ecu):
        w('#include "%s"' % h)
    w('#include "MemMap.h"')
    w()

    # overview table
    w("/* ----------------------------------------------------------------------------------------------")
    w(" * Task map of %s (from OsTask in %s)" % (ecu.name, ecu.ecuc_file))
    w(" *")
    w(" *   task            prio  type      activated by                                  contents")
    for t in ecu.tasks.values():
        act = []
        if t.autostart:
            act.append("autostart")
        for a in t.alarms:
            cyc = (a.get("autostart") or {}).get("cycle", 0)
            act.append("%s/%dms" % (a["name"], cyc * ecu.ecuc["Os"]["OsOS"]["tickDurationUs"] // 1000))
        if any(mp.task == t.name and mp.trigger and mp.trigger.get("type") in ("INTERNAL_WRITE", "COM_NOTIFICATION", "MODE_SWITCH") for mp in ecu.maps):
            act.append("RTE trigger")
        cont = ", ".join(e.runnable.symbol for e in t.entries) if t.entries else ", ".join(t.bsw)
        w(" *   %-15s %-5d %-9s %-45s %s" % (t.name, t.prio, t.type, " + ".join(act), cont))
    w(" * ---------------------------------------------------------------------------------------------- */")
    w()

    for t in ecu.tasks.values():
        w("/* " + "=" * 94)
        w(" * TASK %s   (%s, priority %d, activation limit %d, stack %d words%s)" % (
            t.name, t.type, t.prio, t.act, t.stack, ", autostart in " + "/".join(t.autostart) if t.autostart else ""))
        w(" *")
        if t.entries:
            w(" * Why this task exists: RteEventToTaskMapping (ECUC_Rte_09020) entries in %s put these runnables here:" % ecu.ecuc_file)
            for e in t.entries:
                evs = ", ".join("%s (%s)" % (x.name, event_desc(x)) for x in e.events)
                w(" *   RtePositionInTask %s  runnable %s  <- %s" % (e.position, e.runnable.symbol, evs))
                tr0 = e.maps[0].trigger or {}
                tt = tr0.get("type")
                if tt == "OS_ALARM":
                    al = e.maps[0].alarm
                    a_ = al.get("autostart") or {}
                    w(" *     raised by OsAlarm %s (autostart %s start=%s cycle=%s ticks => RteActivationOffset %s ms, period %s ms): %s%s" % (
                        al["name"], a_.get("type"), a_.get("start"), a_.get("cycle"), a_.get("start"), a_.get("cycle"), al["action"]["type"],
                        (" " + al["action"].get("event", "")) if al["action"]["type"] == "SETEVENT" else ""))
                elif tt == "COM_NOTIFICATION":
                    w(" *     raised by Com notification Rte_COMCbk_%s() (Rte.c) -> %s" % (tr0["comSignal"], "SetEvent(%s)" % tr0.get("osEvent") if t.type == "EXTENDED" else "ActivateTask"))
                elif tt == "INTERNAL_WRITE":
                    w(" *     raised by the on-ECU provider's Rte_Write (Rte.c Rte_Sink_*) -> ActivateTask")
                elif tt == "MODE_SWITCH":
                    w(" *     raised by Rte_Switch_%s_%s (Rte.c) when the mode is entered -> SetEvent(%s)" % (ecu.mode_mgr.port, ecu.mode_mgr.group.name, tr0.get("osEvent")))
                g = _guard_expr(ecu, e)
                if g:
                    w(" *     disabledInMode %s: skipped while the mode is active (the alarm keeps running, only the runnable is suppressed)" % "/".join(e.disabled))
        else:
            w(" * Why this task exists: BSW schedulable entities / init (SWS_Rte_06201). Call order = SchM.tasks[].order in %s:" % ecu.ecuc_file)
            w(" *   %s" % ", ".join(t.bsw))
            for a in t.alarms:
                a_ = a.get("autostart") or {}
                w(" *   raised by OsAlarm %s (%s, start=%s cycle=%s ticks)" % (a["name"], a["action"]["type"], a_.get("start"), a_.get("cycle")))
            if t.autostart:
                w(" *   raised by autostart at StartOS (this is the init task: BSW start-up phase two runs in task context)")
        w(" * " + "=" * 94 + " */")
        w("TASK(%s)" % t.name)
        w("{")
        if t.entries and t.type == "EXTENDED":
            blocks = OrderedDict()
            for e in t.entries:
                blocks.setdefault(e.os_event, []).append(e)
            mask = " | ".join(blocks.keys())
            w("    EventMaskType ev = 0u;")
            w()
            w("    for (;;)                          /* extended task: never terminates, it parks in WaitEvent */")
            w("    {")
            w("        (void)WaitEvent(%s);" % mask)
            w("        (void)GetEvent(%s, &ev);" % t.name)
            w("        (void)ClearEvent(ev);             /* consume what we serve now; events set meanwhile stay pending */")
            w()
            for osev, ents in sorted(blocks.items(), key=lambda kv: min(e.position for e in kv[1])):
                w("        if ((ev & %s) != 0u)" % osev)
                w("        {")
                for e in sorted(ents, key=lambda e: e.position):
                    w("            /* position %s: %s <- %s */" % (e.position, e.runnable.symbol, ", ".join(x.name for x in e.events)))
                    g = _guard_expr(ecu, e)
                    if g:
                        w("            if (%s)         /* disabledInMode */" % g)
                        w("            {")
                        _call_runnable(w, ecu, e, "                ")
                        w("            }")
                    else:
                        _call_runnable(w, ecu, e, "            ")
                w("        }")
            w("    }")
        elif t.entries:
            for e in t.entries:
                w("    /* position %s: %s <- %s */" % (e.position, e.runnable.symbol, ", ".join(x.name for x in e.events)))
                g = _guard_expr(ecu, e)
                if g:
                    w("    if (%s)                /* disabledInMode */" % g)
                    w("    {")
                    _call_runnable(w, ecu, e, "        ")
                    w("    }")
                else:
                    _call_runnable(w, ecu, e, "    ")
            w()
            w("    (void)TerminateTask();            /* basic task: one activation = one run to completion */")
        else:
            for f in t.bsw:
                w("    %s();" % f)
            w()
            w("    (void)TerminateTask();")
        w("}")
        w()

    for i in ecu.ecuc["Os"].get("OsIsr", []):
        w("/* " + "=" * 94)
        w(" * ISR %s (category 2, IRQ %s, NVIC priority %s): %s" % (i["name"], i["irq"], i["nvicPriority"], i.get("_comment", "")))
        w(" * SWS_Rte_04560 / OS: a Cat2 ISR body is generated by the RTE/OS configurator; it only forwards to the driver's ISR body. The")
        w(" * kernel wraps it with ISR_ENTER/ISR_EXIT, interrupt-priority handling and the reschedule check on exit.")
        w(" * " + "=" * 94 + " */")
        w("MINI_CODE_FAST ISR(%s)" % i["name"])
        w("{")
        for f in i["calls"]:
            w("    %s();" % f)
        w("}")
        w()
    return w.text()
