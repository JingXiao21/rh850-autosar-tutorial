"""
arxml_model.py - input side of the mini RTE generator.

[Educational Implementation]
Real AUTOSAR counterpart: the *input readers + consistency checks* of an RTE generator / BSW configurator
(Vector DaVinci, EB tresos, ...): they read the SW-component descriptions (SwcTypes.arxml), the system
description (System.arxml) and the ECU configuration (ECUC) and refuse to generate when the three do not
agree.  Here:

    load_inputs()   parse the ARXML subset (xml.etree) and the ECUC JSON into plain Python objects
    resolve()       build the per-ECU view: which SWC prototypes live on the ECU, how every S/R element is
                    connected (Com signal or RTE-internal buffer), which server a client port calls, which
                    runnable runs in which OS task and in which order, which Exclusive Area needs which
                    OS-resource ceiling ...
    every inconsistency is collected as ONE readable message ("<where>: <what is wrong> (<how to fix>)")
    and raised together as GenError, so a configuration author sees all problems in one run.
"""
import json
import re
import sys
import xml.etree.ElementTree as ET
from collections import OrderedDict


class GenError(Exception):
    """Raised with all collected validation errors."""

    def __init__(self, errors):
        self.errors = list(errors)
        super().__init__("\n".join("ERROR: " + e for e in self.errors))


# ------------------------------------------------------------------------------------------------ helpers
def _t(el, tag, default=None):
    """text of a direct child element."""
    c = el.find(tag)
    if c is None or c.text is None:
        return default
    return c.text.strip()


def _last(ref):
    """'/Interfaces/WheelSpeedIf' -> 'WheelSpeedIf' (references are plain absolute paths in this ARXML subset)."""
    return ref.strip().rsplit("/", 1)[-1] if ref else ref


def _int(text):
    return int(str(text).strip(), 0)


def c_hex(v):
    return "0x%Xu" % v


C_TYPES = {"uint8": 8, "uint16": 16, "uint32": 32, "sint8": 8, "sint16": 16, "sint32": 32, "boolean": 8}
COM_TYPE_OF = {"uint8": "UINT8", "uint16": "UINT16", "uint32": "UINT32",
               "sint8": "SINT8", "sint16": "SINT16", "sint32": "SINT32", "boolean": "BOOLEAN"}


# ------------------------------------------------------------------------------------------------ model objects
class Obj(object):
    """tiny attribute bag (keeps the model readable without a dataclass zoo)."""

    def __init__(self, **kw):
        self.__dict__.update(kw)

    def __repr__(self):
        return "Obj(%s)" % ", ".join("%s=%r" % kv for kv in sorted(self.__dict__.items()))


class Inputs(object):
    pass


def _parse_swc_types(path):
    root = ET.parse(path).getroot()
    m = Obj(dtypes=OrderedDict(), modegroups=OrderedDict(), ifaces=OrderedDict(), swcs=OrderedDict())

    for e in root.iter("IMPLEMENTATION-DATA-TYPE"):
        m.dtypes[_t(e, "SHORT-NAME")] = _t(e, "BASE-TYPE")

    for e in root.iter("MODE-DECLARATION-GROUP"):
        modes = OrderedDict()
        for d in e.iter("MODE-DECLARATION"):
            modes[_t(d, "SHORT-NAME")] = _int(_t(d, "VALUE"))
        m.modegroups[_t(e, "SHORT-NAME")] = Obj(name=_t(e, "SHORT-NAME"), initial=_last(_t(e, "INITIAL-MODE-REF")), modes=modes)

    for e in root.iter("SENDER-RECEIVER-INTERFACE"):
        els = OrderedDict()
        for v in e.iter("VARIABLE-DATA-PROTOTYPE"):
            els[_t(v, "SHORT-NAME")] = Obj(name=_t(v, "SHORT-NAME"), type=_last(_t(v, "TYPE-TREF")), init=_int(_t(v, "INIT-VALUE", "0")))
        m.ifaces[_t(e, "SHORT-NAME")] = Obj(kind="SR", name=_t(e, "SHORT-NAME"), elements=els)

    for e in root.iter("CLIENT-SERVER-INTERFACE"):
        ops = OrderedDict()
        for o in e.iter("CLIENT-SERVER-OPERATION"):
            args = [Obj(name=_t(a, "SHORT-NAME"), type=_last(_t(a, "TYPE-TREF")), dir=_t(a, "DIRECTION", "IN"))
                    for a in o.iter("ARGUMENT-DATA-PROTOTYPE")]
            ops[_t(o, "SHORT-NAME")] = Obj(name=_t(o, "SHORT-NAME"), args=args)
        m.ifaces[_t(e, "SHORT-NAME")] = Obj(kind="CS", name=_t(e, "SHORT-NAME"), ops=ops)

    for e in root.iter("MODE-SWITCH-INTERFACE"):
        g = e.find("MODE-GROUP")
        m.ifaces[_t(e, "SHORT-NAME")] = Obj(kind="MS", name=_t(e, "SHORT-NAME"), group=_t(g, "SHORT-NAME"), group_type=_last(_t(g, "TYPE-TREF")))

    for tag in ("APPLICATION-SW-COMPONENT-TYPE", "SERVICE-SW-COMPONENT-TYPE"):
        for e in root.iter(tag):
            ports = OrderedDict()
            for p in e.find("PORTS"):
                if p.tag == "P-PORT-PROTOTYPE":
                    ports[_t(p, "SHORT-NAME")] = Obj(name=_t(p, "SHORT-NAME"), dir="P", iface=_last(_t(p, "PROVIDED-INTERFACE-TREF")))
                elif p.tag == "R-PORT-PROTOTYPE":
                    ports[_t(p, "SHORT-NAME")] = Obj(name=_t(p, "SHORT-NAME"), dir="R", iface=_last(_t(p, "REQUIRED-INTERFACE-TREF")))
            sw = Obj(name=_t(e, "SHORT-NAME"), service=(tag == "SERVICE-SW-COMPONENT-TYPE"), ports=ports, pims=[], eas=[],
                     runnables=OrderedDict(), events=[])
            for ib in e.iter("SWC-INTERNAL-BEHAVIOR"):
                for p in ib.iter("PER-INSTANCE-MEMORY"):
                    sw.pims.append(Obj(name=_t(p, "SHORT-NAME"), type=_t(p, "TYPE"),
                                       fields=[(_t(f, "NAME"), _t(f, "TYPE")) for f in p.iter("FIELD")],
                                       init=_t(p, "INIT-VALUE", "{0}")))
                for a in ib.iter("EXCLUSIVE-AREA"):
                    sw.eas.append(_t(a, "SHORT-NAME"))
                for r in ib.iter("RUNNABLE-ENTITY"):
                    def va(container):
                        out = []
                        c = r.find(container)
                        if c is not None:
                            for v in c.iter("VARIABLE-ACCESS"):
                                out.append((_t(v, "PORT-REF"), _t(v, "DATA-ELEMENT-REF")))
                        return out
                    calls = []
                    c = r.find("SERVER-CALL-POINTS")
                    if c is not None:
                        for v in c:
                            calls.append((_t(v, "PORT-REF"), _t(v, "OPERATION-REF")))
                    modes = []
                    c = r.find("MODE-ACCESS-POINTS")
                    if c is not None:
                        for v in c.iter("MODE-ACCESS-POINT"):
                            modes.append((_t(v, "PORT-REF"), _t(v, "MODE-GROUP-REF")))
                    eas = [x.text.strip() for x in r.iter("CAN-ENTER-EXCLUSIVE-AREA-REF")]
                    eas += [x.text.strip() for x in r.iter("RUNS-INSIDE-EXCLUSIVE-AREA-REF")]
                    rn = Obj(name=_t(r, "SHORT-NAME"), symbol=_t(r, "SYMBOL", _t(r, "SHORT-NAME")),
                             concurrent=(_t(r, "CAN-BE-INVOKED-CONCURRENTLY", "false") == "true"),
                             implicit_writes=va("DATA-WRITE-ACCESSS"), implicit_reads=va("DATA-READ-ACCESSS"),
                             explicit_reads=va("DATA-RECEIVE-POINT-BY-ARGUMENTS"), explicit_writes=va("DATA-SEND-POINTS"),
                             calls=calls, modes=modes, eas=eas)
                    sw.runnables[rn.name] = rn
                ev = ib.find("EVENTS")
                if ev is not None:
                    for x in ev:
                        o = Obj(name=_t(x, "SHORT-NAME"), kind=None, runnable=_t(x, "START-ON-EVENT-REF"), swc=sw.name,
                                period_ms=None, offset_ms=None, disabled=[], data=None, mode=None, activation=None, op=None)
                        if x.tag == "TIMING-EVENT":
                            o.kind = "TIMING"
                            o.period_ms = int(round(float(_t(x, "PERIOD")) * 1000))
                            if _t(x, "OFFSET") is not None:
                                o.offset_ms = int(round(float(_t(x, "OFFSET")) * 1000))
                            for d in x.iter("DISABLED-MODE-IREF"):
                                o.disabled.append((_t(d, "PORT-REF"), _last(_t(d, "MODE-REF"))))
                        elif x.tag == "DATA-RECEIVED-EVENT":
                            o.kind = "DATA_RECEIVED"
                            d = x.find("DATA-IREF")
                            o.data = (_t(d, "PORT-REF"), _t(d, "DATA-ELEMENT-REF"))
                        elif x.tag == "SWC-MODE-SWITCH-EVENT":
                            o.kind = "MODE_SWITCH"
                            o.activation = _t(x, "ACTIVATION", "ON-ENTRY")
                            d = x.find("MODE-IREF")
                            o.mode = (_t(d, "PORT-REF"), _last(_t(d, "MODE-REF")))
                        elif x.tag == "OPERATION-INVOKED-EVENT":
                            o.kind = "OPERATION_INVOKED"
                            d = x.find("OPERATION-IREF")
                            o.op = (_t(d, "PORT-REF"), _t(d, "OPERATION-REF"))
                        else:
                            continue
                        sw.events.append(o)
            m.swcs[sw.name] = sw
    return m


def _parse_system(path):
    root = ET.parse(path).getroot()
    s = Obj(ecus=OrderedDict(), signals=OrderedDict(), pdus=OrderedDict(), frames=OrderedDict(), protos=OrderedDict(),
            connectors=[], swc_ecu=OrderedDict(), sr_map=OrderedDict())
    for e in root.iter("ECU-INSTANCE"):
        s.ecus[_t(e, "SHORT-NAME")] = Obj(name=_t(e, "SHORT-NAME"), generate=(_t(e, "GENERATE", "true") != "false"))
    for e in root.iter("I-SIGNAL"):
        s.signals[_t(e, "SHORT-NAME")] = Obj(name=_t(e, "SHORT-NAME"), length=_int(_t(e, "LENGTH")), type=_t(e, "TYPE"), init=_int(_t(e, "INIT-VALUE", "0")))
    for e in root.iter("I-SIGNAL-I-PDU"):
        maps = [(_last(_t(x, "I-SIGNAL-REF")), _int(_t(x, "START-POSITION"))) for x in e.iter("I-SIGNAL-TO-I-PDU-MAPPING")]
        tm = e.find("I-PDU-TIMING")
        s.pdus[_t(e, "SHORT-NAME")] = Obj(name=_t(e, "SHORT-NAME"), length=_int(_t(e, "LENGTH")), signals=maps,
                                          period_ms=(int(round(float(_t(tm, "PERIOD", "0")) * 1000)) if tm is not None else None))
    for e in root.iter("CAN-FRAME"):
        s.frames[_t(e, "SHORT-NAME")] = Obj(name=_t(e, "SHORT-NAME"), pdu=_last(_t(e, "PDU-REF")), can_id=_int(_t(e, "IDENTIFIER")), length=_int(_t(e, "FRAME-LENGTH")))
    for e in root.iter("SW-COMPONENT-PROTOTYPE"):
        s.protos[_t(e, "SHORT-NAME")] = _last(_t(e, "TYPE-TREF"))
    for e in root.iter("ASSEMBLY-SW-CONNECTOR"):
        pr, rq = e.find("PROVIDER-IREF"), e.find("REQUESTER-IREF")
        s.connectors.append(Obj(name=_t(e, "SHORT-NAME"), prov=(_t(pr, "COMPONENT"), _t(pr, "PORT")), req=(_t(rq, "COMPONENT"), _t(rq, "PORT"))))
    for e in root.iter("SWC-TO-ECU-MAPPING"):
        s.swc_ecu.setdefault(_t(e, "COMPONENT"), set()).add(_last(_t(e, "ECU-REF")))
    for e in root.iter("SENDER-RECEIVER-TO-SIGNAL-MAPPING"):
        s.sr_map[(_t(e, "COMPONENT"), _t(e, "PORT"), _t(e, "DATA-ELEMENT"))] = _last(_t(e, "I-SIGNAL-REF"))
    return s


def load_inputs(swc_path, system_path, ecuc_path):
    inp = Inputs()
    inp.swc = _parse_swc_types(swc_path)
    inp.sys = _parse_system(system_path)
    with open(ecuc_path, encoding="utf-8") as f:
        inp.ecuc = json.load(f, object_pairs_hook=OrderedDict)
    if inp.ecuc.get("schema") != "mini-ecuc/1":
        raise GenError(["%s: schema must be 'mini-ecuc/1'" % ecuc_path])
    return inp


# ------------------------------------------------------------------------------------------------ resolved ECU view
def parse_c_int(v):
    """JSON values may be ints or strings like '0x101'."""
    if isinstance(v, int):
        return v
    return int(str(v), 0)


def id_name(container, name):
    """ModConf-style id: PortPin 'Pin_Usart1Tx' -> 'PortPin_Pin_Usart1Tx'; strips a redundant container prefix
    (McuClockSettingConfig_0 -> McuClockSettingConfig_0, not McuClockSettingConfig_McuClockSettingConfig_0)."""
    if name.startswith(container + "_"):
        name = name[len(container) + 1:]
    return container + "_" + name


def resolve(inp):
    """Cross-check the three inputs and build the per-ECU view used by all emitters."""
    errs = []

    def err(where, msg):
        errs.append("%s: %s" % (where, msg))

    swc, sy, ec = inp.swc, inp.sys, inp.ecuc
    ecu_name = ec["Ecu"]["name"]
    ew = ecu_name + ".ecuc.json"
    ecu = Obj(name=ecu_name, define=ec["Ecu"]["define"], tag=ec["Ecu"]["tag"], ecuc=ec, swc=swc, sys=sy,
              ecuc_file="config/ecuc/%s.ecuc.json" % ecu_name)

    if ecu_name not in sy.ecus:
        err("System.arxml", "ECU-INSTANCE '%s' not found" % ecu_name)

    # ---- which SWC prototypes live here -------------------------------------------------------------------
    protos = []
    for p in ec["Ecu"]["swcPrototypes"]:
        if p not in sy.protos:
            err(ew, "swcPrototypes: '%s' is not a SW-COMPONENT-PROTOTYPE of the root composition" % p)
            continue
        if ecu_name not in sy.swc_ecu.get(p, set()):
            err(ew, "swcPrototypes: '%s' is not mapped to ECU '%s' in System.arxml (SWC-TO-ECU-MAPPING)" % (p, ecu_name))
        t = sy.protos[p]
        if t not in swc.swcs:
            err("System.arxml", "prototype '%s' references unknown SWC type '%s'" % (p, t))
            continue
        protos.append(Obj(name=p, type=swc.swcs[t]))
    ecu.protos = protos
    pmap = OrderedDict((p.name, p) for p in protos)
    ecu.pmap = pmap
    ecu.apps = [p for p in protos if not p.type.service]

    def iface_of(proto, port):
        pt = pmap[proto].type
        if port not in pt.ports:
            return None
        return swc.ifaces.get(pt.ports[port].iface)

    # ---- data types ----------------------------------------------------------------------------------------
    def base(tn):
        return swc.dtypes.get(tn, tn)

    for iname, i in swc.ifaces.items():
        if i.kind == "SR":
            for e in i.elements.values():
                if e.type not in swc.dtypes:
                    err("SwcTypes.arxml", "interface %s element %s: unknown data type '%s'" % (iname, e.name, e.type))
        if i.kind == "CS":
            for o in i.ops.values():
                for a in o.args:
                    if a.type not in swc.dtypes:
                        err("SwcTypes.arxml", "interface %s operation %s argument %s: unknown data type '%s'" % (iname, o.name, a.name, a.type))

    # ---- OS objects ----------------------------------------------------------------------------------------
    os_ = ec["Os"]
    tasks = OrderedDict()
    for t in os_["OsTask"]:
        tasks[t["name"]] = Obj(name=t["name"], prio=t["priority"], act=t.get("activation", 1), type=t["type"], sched=t.get("schedule", "FULL"),
                               autostart=t.get("autostart", []), stack=t.get("stackWords", 256), events=t.get("events", []),
                               entries=[], bsw=[], alarms=[], index=len(tasks))
    ecu.tasks = tasks
    ev_masks = OrderedDict((e["name"], parse_c_int(e["mask"])) for e in os_.get("OsEvent", []))
    ecu.ev_masks = ev_masks
    alarms = OrderedDict((a["name"], a) for a in os_.get("OsAlarm", []))
    ecu.alarms = alarms
    tick_us = os_["OsOS"]["tickDurationUs"]

    prios = {}
    for t in tasks.values():
        if not (1 <= t.prio <= 255):
            err(ew, "OsTask %s: priority %d out of range 1..255 (0 is the idle task)" % (t.name, t.prio))
        if t.prio in prios:
            err(ew, "OsTask priority conflict: %s and %s both have priority %d (this project requires unique priorities so that the "
                    "schedule is unambiguous; change one of them)" % (prios[t.prio], t.name, t.prio))
        prios[t.prio] = t.name
        if t.type not in ("BASIC", "EXTENDED"):
            err(ew, "OsTask %s: type must be BASIC or EXTENDED" % t.name)
        if t.type == "BASIC" and t.events:
            err(ew, "OsTask %s is BASIC but lists OsTaskEvents %s (only extended tasks own events)" % (t.name, t.events))
        if t.type == "EXTENDED" and t.act != 1:
            err(ew, "OsTask %s: extended tasks must have activation 1" % t.name)
        for e in t.events:
            if e not in ev_masks:
                err(ew, "OsTask %s lists unknown OsEvent '%s'" % (t.name, e))
    seen_bits = {}
    for n, mk in ev_masks.items():
        if mk == 0 or (mk & (mk - 1)):
            err(ew, "OsEvent %s: mask 0x%x must be exactly one bit" % (n, mk))
        if mk in seen_bits:
            err(ew, "OsEvent %s and %s share mask 0x%x" % (seen_bits[mk], n, mk))
        seen_bits[mk] = n
    for a in alarms.values():
        tgt = a["action"].get("task")
        if a["action"]["type"] in ("ACTIVATETASK", "SETEVENT"):
            if tgt not in tasks:
                err(ew, "OsAlarm %s targets unknown task '%s'" % (a["name"], tgt))
            else:
                tasks[tgt].alarms.append(a)
    for i in os_.get("OsIsr", []):
        if i.get("category", 2) != 2:
            err(ew, "OsIsr %s: only category 2 ISRs are supported" % i["name"])

    # ---- Com signals of this ECU ---------------------------------------------------------------------------
    com = ec["Com"]
    ipdus = OrderedDict((p["name"], p) for p in com["ComIPdu"])
    csigs = OrderedDict((s["name"], s) for s in com["ComSignal"])
    ecu.ipdus, ecu.csigs = ipdus, csigs
    for s in csigs.values():
        if s["ipdu"] not in ipdus:
            err(ew, "ComSignal %s references unknown ComIPdu '%s'" % (s["name"], s["ipdu"]))
            continue
        if s["name"] not in sy.signals:
            err(ew, "ComSignal %s has no I-SIGNAL of that name in System.arxml" % s["name"])
            continue
        isig = sy.signals[s["name"]]
        if isig.length != s["bitSize"]:
            err(ew, "ComSignal %s: ComBitSize %d != I-SIGNAL LENGTH %d in System.arxml" % (s["name"], s["bitSize"], isig.length))
        if COM_TYPE_OF.get(isig.type) != s["type"]:
            err(ew, "ComSignal %s: ComSignalType %s does not match I-SIGNAL type %s" % (s["name"], s["type"], isig.type))
        if isig.init != s["init"]:
            err(ew, "ComSignal %s: init %d != I-SIGNAL INIT-VALUE %d" % (s["name"], s["init"], isig.init))
        ip = ipdus[s["ipdu"]]
        cid = parse_c_int(ip["canId"])
        fr = [f for f in sy.frames.values() if f.can_id == cid]
        if not fr:
            err(ew, "ComIPdu %s: CAN id 0x%X is not a CAN-FRAME of System.arxml" % (ip["name"], cid))
        else:
            pdu = sy.pdus.get(fr[0].pdu)
            hit = [pos for (sn, pos) in pdu.signals if sn == s["name"]] if pdu else []
            if not hit:
                err(ew, "ComSignal %s is not packed in %s (frame 0x%X) in System.arxml" % (s["name"], fr[0].pdu, cid))
            elif hit[0] != s["bitPosition"]:
                err(ew, "ComSignal %s: bitPosition %d != START-POSITION %d in System.arxml" % (s["name"], s["bitPosition"], hit[0]))
            if pdu and pdu.length != ip["lengthBytes"]:
                err(ew, "ComIPdu %s: lengthBytes %d != PDU LENGTH %d in System.arxml" % (ip["name"], ip["lengthBytes"], pdu.length))
            if pdu and ip["direction"] == "TX" and pdu.period_ms is not None and ip.get("txPeriodMs") != pdu.period_ms:
                err(ew, "ComIPdu %s: txPeriodMs %s != I-PDU-TIMING PERIOD %d ms in System.arxml" % (ip["name"], ip.get("txPeriodMs"), pdu.period_ms))
    # I-PDU group names
    groups = [g["name"] for g in com.get("ComIPduGroup", [])]
    for p in ipdus.values():
        if p.get("group") not in groups:
            err(ew, "ComIPdu %s references unknown ComIPduGroup '%s'" % (p["name"], p.get("group")))

    # ---- S/R connectivity ----------------------------------------------------------------------------------
    # providers feeding an on-ECU requester get an RTE buffer; mapped signals use Com.
    sources = OrderedDict()   # (proto, port, elem) -> Obj(kind 'COM'|'BUF', signal | key)
    sinks = OrderedDict()     # (proto, port, elem) -> Obj(type, signal, buf, triggers[])
    ecu.sources, ecu.sinks = sources, sinks

    def elem_info(proto, port, elem, where):
        i = iface_of(proto, port)
        if i is None or i.kind != "SR" or elem not in i.elements:
            err(where, "%s.%s.%s is not an S/R data element" % (proto, port, elem))
            return None
        return i.elements[elem]

    def conns_for_req(proto, port):
        return [c for c in sy.connectors if c.req == (proto, port)]

    def conns_for_prov(proto, port):
        return [c for c in sy.connectors if c.prov == (proto, port)]

    def sink_of(proto, port, elem, ei):
        k = (proto, port, elem)
        if k not in sinks:
            sinks[k] = Obj(key=k, type=ei.type, init=ei.init, signal=None, buf=None, triggers=[], proto=proto, port=port, elem=elem,
                           written=False)
        return sinks[k]

    def resolve_source(proto, port, elem, where):
        """where does Rte_Read / Rte_IRead of R-port element get its data from?"""
        k = (proto, port, elem)
        if k in sources:
            return sources[k]
        ei = elem_info(proto, port, elem, where)
        if ei is None:
            return None
        sig = sy.sr_map.get(k)
        src = None
        if sig is not None:
            cs = csigs.get(sig)
            if cs is None:
                err(where, "%s.%s.%s is mapped to I-SIGNAL '%s' but ECU '%s' has no such ComSignal (RX signal missing in %s)" % (proto, port, elem, sig, ecu_name, ew))
                return None
            if ipdus.get(cs["ipdu"], {}).get("direction") != "RX":
                err(where, "R-port %s.%s receives signal '%s' but ComIPdu %s is a TX I-PDU on this ECU" % (proto, port, sig, cs["ipdu"]))
            if COM_TYPE_OF.get(base(ei.type)) != cs["type"]:
                err(where, "data type mismatch: %s.%s.%s is %s but ComSignal %s is %s" % (proto, port, elem, ei.type, sig, cs["type"]))
            src = Obj(kind="COM", signal=sig, type=ei.type, init=ei.init)
        else:
            for c in conns_for_req(proto, port):
                if c.prov[0] in pmap:
                    pk = (c.prov[0], c.prov[1], elem)
                    pe = elem_info(c.prov[0], c.prov[1], elem, where)
                    if pe is None:
                        return None
                    if pe.type != ei.type:
                        err(where, "data type mismatch across connector %s: provider %s is %s, requester %s is %s" % (c.name, pk, pe.type, k, ei.type))
                    sk = sink_of(c.prov[0], c.prov[1], elem, pe)
                    sk.buf = "Rte_Buf_%s_%s_%s" % (c.prov[0], c.prov[1], elem)
                    src = Obj(kind="BUF", buf=sk.buf, sink=sk, type=ei.type, init=ei.init)
                    break
        sources[k] = src
        return src

    def resolve_sink(proto, port, elem, where):
        k = (proto, port, elem)
        ei = elem_info(proto, port, elem, where)
        if ei is None:
            return None
        sk = sink_of(proto, port, elem, ei)
        sig = sy.sr_map.get(k)
        if sig is not None and sk.signal is None:
            cs = csigs.get(sig)
            if cs is None:
                err(where, "%s.%s.%s is mapped to I-SIGNAL '%s' but ECU '%s' has no such ComSignal (TX signal missing in %s)" % (proto, port, elem, sig, ecu_name, ew))
            else:
                if ipdus.get(cs["ipdu"], {}).get("direction") != "TX":
                    err(where, "P-port %s.%s sends signal '%s' but ComIPdu %s is an RX I-PDU on this ECU" % (proto, port, sig, cs["ipdu"]))
                if COM_TYPE_OF.get(base(ei.type)) != cs["type"]:
                    err(where, "data type mismatch: %s.%s.%s is %s but ComSignal %s is %s" % (proto, port, elem, ei.type, sig, cs["type"]))
                sk.signal = sig
        for c in conns_for_prov(proto, port):
            if c.req[0] in pmap:
                rp_if = iface_of(c.req[0], c.req[1])
                pp_if = iface_of(proto, port)
                if rp_if is None or pp_if is None or rp_if.name != pp_if.name:
                    err("System.arxml", "connector %s joins ports with different interfaces (%s vs %s)" % (
                        c.name, pp_if.name if pp_if else "?", rp_if.name if rp_if else "?"))
                    continue
                if sy.sr_map.get((c.req[0], c.req[1], elem)) is None:
                    sk.buf = "Rte_Buf_%s_%s_%s" % (proto, port, elem)
        return sk

    # ---- per runnable access resolution --------------------------------------------------------------------
    ecu.runnable_info = OrderedDict()    # (proto, runnable name) -> Obj
    mode_mgr = ec["Rte"].get("RteModeManager")
    ecu.mode_mgr = None
    if mode_mgr:
        if mode_mgr["swc"] not in pmap:
            err(ew, "RteModeManager: SWC prototype '%s' not on this ECU" % mode_mgr["swc"])
        elif mode_mgr["modeGroup"] not in swc.modegroups:
            err(ew, "RteModeManager: unknown mode group '%s'" % mode_mgr["modeGroup"])
        else:
            ecu.mode_mgr = Obj(proto=mode_mgr["swc"], port=mode_mgr["port"], group=swc.modegroups[mode_mgr["modeGroup"]],
                               initial=mode_mgr.get("initialMode", swc.modegroups[mode_mgr["modeGroup"]].initial))

    bsw_servers = {(m["swc"], m["port"], m["operation"]): m for m in ec["Rte"].get("RteBswServerMapping", [])}
    ecu.bsw_servers = bsw_servers

    def server_runnable(proto, port, op, where):
        """What does Rte_Call_<port>_<op> of a client port end up calling?  -> Obj(kind 'RUNNABLE'|'BSW', ...)"""
        k = (proto, port, op)
        if k in bsw_servers:
            m = bsw_servers[k]
            return Obj(kind="BSW", function=m["function"], header=m.get("header"))
        cons = conns_for_req(proto, port)
        for c in cons:
            if c.prov[0] in pmap:
                st = pmap[c.prov[0]].type
                for ev in st.events:
                    if ev.kind == "OPERATION_INVOKED" and ev.op == (c.prov[1], op):
                        rn = st.runnables[ev.runnable]
                        return Obj(kind="RUNNABLE", proto=c.prov[0], runnable=rn, event=ev)
                err(where, "client port %s.%s calls '%s' but server port %s.%s has no OperationInvokedEvent for it" % (proto, port, op, c.prov[0], c.prov[1]))
                return None
        err(where, "client port %s.%s (operation %s) is not connected: no ASSEMBLY-SW-CONNECTOR to a server on this ECU and no "
                   "RteBswServerMapping in %s" % (proto, port, op, ew))
        return None

    for p in ecu.apps:
        st = p.type
        for rn in st.runnables.values():
            where = "%s/%s" % (st.name, rn.name)
            ri = Obj(proto=p.name, swc_type=st.name, runnable=rn, rd=[], wr=[], ird=[], iwr=[], calls=[], modes=[], eas=[], is_server=False)
            for (port, elem) in rn.explicit_reads:
                if port not in st.ports or st.ports[port].dir != "R":
                    err(where, "explicit read of %s.%s: not an R-port" % (port, elem))
                    continue
                src = resolve_source(p.name, port, elem, where)
                if src is None:
                    if (p.name, port, elem) in sources or iface_of(p.name, port):
                        err(where, "port %s.%s.%s is read by runnable %s but not connected (no ASSEMBLY-SW-CONNECTOR from a provider on this ECU "
                                   "and no S/R-to-signal mapping in System.arxml)" % (p.name, port, elem, rn.name))
                    continue
                ri.rd.append((port, elem, src))
            for (port, elem) in rn.implicit_reads:
                if port not in st.ports or st.ports[port].dir != "R":
                    err(where, "implicit read of %s.%s: not an R-port" % (port, elem))
                    continue
                src = resolve_source(p.name, port, elem, where)
                if src is None:
                    err(where, "port %s.%s.%s is read (implicit) by runnable %s but not connected" % (p.name, port, elem, rn.name))
                    continue
                ri.ird.append((port, elem, src))
            for (port, elem) in rn.explicit_writes + rn.implicit_writes:
                if port not in st.ports or st.ports[port].dir != "P":
                    err(where, "write of %s.%s: not a P-port" % (port, elem))
                    continue
                sk = resolve_sink(p.name, port, elem, where)
                if sk is None:
                    continue
                if sk.signal is None and sk.buf is None:
                    err(where, "port %s.%s.%s is written by runnable %s but not connected (no signal mapping, no connector to an on-ECU requester)" % (
                        p.name, port, elem, rn.name))
                    continue
                sk.written = True
                (ri.wr if (port, elem) in rn.explicit_writes else ri.iwr).append((port, elem, sk))
            for (port, op) in rn.calls:
                if port not in st.ports or st.ports[port].dir != "R":
                    err(where, "call point %s.%s: not an R-port" % (port, op))
                    continue
                i = iface_of(p.name, port)
                if i is None or i.kind != "CS" or op not in i.ops:
                    err(where, "call point %s.%s: not a client/server operation" % (port, op))
                    continue
                srv = server_runnable(p.name, port, op, where)
                if srv is not None:
                    ri.calls.append((port, i.ops[op], srv))
            for (port, grp) in rn.modes:
                if port not in st.ports:
                    err(where, "mode access point on unknown port '%s'" % port)
                    continue
                i = iface_of(p.name, port)
                if i is None or i.kind != "MS":
                    err(where, "mode access point %s: not a mode switch interface" % port)
                    continue
                prov = [c for c in conns_for_req(p.name, port) if ecu.mode_mgr and c.prov == (ecu.mode_mgr.proto, ecu.mode_mgr.port)]
                if not prov:
                    err(where, "mode port %s.%s is read by runnable %s but not connected to the mode manager %s.%s" % (
                        p.name, port, rn.name, mode_mgr["swc"] if mode_mgr else "?", mode_mgr["port"] if mode_mgr else "?"))
                    continue
                ri.modes.append((port, grp))
            ri.eas = list(rn.eas)
            ecu.runnable_info[(p.name, rn.name)] = ri
    # server runnables = those started by an OperationInvokedEvent
    for p in ecu.apps:
        for ev in p.type.events:
            if ev.kind == "OPERATION_INVOKED":
                ecu.runnable_info[(p.name, ev.runnable)].is_server = True

    # ports accessed by no runnable but not connected -> warning only
    for p in ecu.apps:
        used = set()
        for rn in p.type.runnables.values():
            for (a, _) in rn.explicit_reads + rn.implicit_reads + rn.explicit_writes + rn.implicit_writes:
                used.add(a)
            for (a, _) in rn.calls + rn.modes:
                used.add(a)
        for ev in p.type.events:
            if ev.data:
                used.add(ev.data[0])
            if ev.op:
                used.add(ev.op[0])
        for pn, pt in p.type.ports.items():
            if pn not in used:
                sys.stderr.write("WARNING: %s.%s: port is not used by any runnable (ignored)\n" % (p.name, pn))

    # ---- RteEventToTaskMapping ----------------------------------------------------------------------------
    maps = ec["Rte"]["RteEventToTaskMapping"]
    ecu.maps = []
    mapped = {}
    for m in maps:
        where = "%s: RteEventToTaskMapping[%s/%s]" % (ew, m.get("swc"), m.get("event"))
        pr = pmap.get(m["swc"])
        if pr is None:
            err(where, "SWC prototype '%s' is not on this ECU" % m["swc"])
            continue
        evs = [e for e in pr.type.events if e.name == m["event"]]
        if not evs:
            err(where, "event '%s' does not exist in SWC type %s" % (m["event"], pr.type.name))
            continue
        ev = evs[0]
        if ev.runnable != m["runnable"]:
            err(where, "mapping says runnable '%s' but the ARXML event starts '%s'" % (m["runnable"], ev.runnable))
            continue
        if ev.runnable not in pr.type.runnables:
            err(where, "runnable '%s' not defined in SWC type %s" % (ev.runnable, pr.type.name))
            continue
        if (m["swc"], m["event"]) in mapped:
            err(where, "event mapped twice")
        mapped[(m["swc"], m["event"])] = m
        rn = pr.type.runnables[ev.runnable]
        mp = Obj(proto=m["swc"], event=ev, runnable=rn, task=m.get("task"), position=m.get("position"), trigger=m.get("trigger"), raw=m,
                 info=ecu.runnable_info[(m["swc"], rn.name)])
        if mp.task is None:
            if ev.kind != "OPERATION_INVOKED":
                err(where, "only OperationInvokedEvents may have task=null (direct call); %s event '%s' needs a task" % (ev.kind, ev.name))
            ecu.maps.append(mp)
            continue
        if mp.task not in tasks:
            err(where, "task '%s' is not an OsTask of this ECU" % mp.task)
            continue
        if mp.position is None:
            err(where, "RtePositionInTask missing")
        if ev.kind == "OPERATION_INVOKED":
            err(where, "OperationInvokedEvent mapped to a task: asynchronous/cross-task client-server is not supported by this RTE "
                       "(use task=null for a synchronous direct call)")
            continue
        tr = mp.trigger or {}
        tt = tr.get("type")
        task = tasks[mp.task]
        if ev.kind == "TIMING":
            if tt != "OS_ALARM":
                err(where, "TimingEvent needs trigger OS_ALARM")
            else:
                al = alarms.get(tr.get("alarm"))
                if al is None:
                    err(where, "trigger alarm '%s' is not an OsAlarm" % tr.get("alarm"))
                else:
                    cyc = (al.get("autostart") or {}).get("cycle", 0)
                    if cyc * tick_us != ev.period_ms * 1000:
                        err(where, "TimingEvent %s period %d ms != alarm %s cycle %d ticks x %d us" % (ev.name, ev.period_ms, al["name"], cyc, tick_us))
                    if al["action"].get("task") != mp.task:
                        err(where, "alarm %s activates task '%s', not '%s'" % (al["name"], al["action"].get("task"), mp.task))
                    if task.type == "EXTENDED":
                        if al["action"]["type"] != "SETEVENT" or al["action"].get("event") != tr.get("osEvent"):
                            err(where, "alarm %s must SETEVENT %s on extended task %s" % (al["name"], tr.get("osEvent"), mp.task))
                    elif al["action"]["type"] != "ACTIVATETASK":
                        err(where, "alarm %s must ACTIVATETASK basic task %s" % (al["name"], mp.task))
                    mp.alarm = al
        elif ev.kind == "DATA_RECEIVED":
            port, elem = ev.data
            src = resolve_source(m["swc"], port, elem, where)
            if src is None:
                err(where, "DataReceivedEvent %s: port %s.%s is not connected" % (ev.name, m["swc"], port))
            elif src.kind == "COM":
                if tt != "COM_NOTIFICATION" or tr.get("comSignal") != src.signal:
                    err(where, "DataReceivedEvent %s receives Com signal '%s': trigger must be COM_NOTIFICATION with comSignal '%s'" % (ev.name, src.signal, src.signal))
                else:
                    cs = csigs[src.signal]
                    if cs.get("notification") != "Rte_COMCbk_%s" % src.signal:
                        err(where, "ComSignal %s must have notification 'Rte_COMCbk_%s' for this event (found %r)" % (src.signal, src.signal, cs.get("notification")))
            else:
                if tt != "INTERNAL_WRITE":
                    err(where, "DataReceivedEvent %s is fed by an on-ECU provider: trigger must be INTERNAL_WRITE" % ev.name)
                else:
                    src.sink.triggers.append(mp)
        elif ev.kind == "MODE_SWITCH":
            if tt != "MODE_SWITCH":
                err(where, "ModeSwitchEvent needs trigger MODE_SWITCH")
            if not ecu.mode_mgr:
                err(where, "ModeSwitchEvent without RteModeManager")
        else:
            err(where, "unsupported event kind %s" % ev.kind)
        if task.type == "EXTENDED":
            if tr.get("osEvent") not in task.events:
                err(where, "task %s is EXTENDED: trigger osEvent '%s' must be one of its OsTaskEvents %s" % (task.name, tr.get("osEvent"), task.events))
        ecu.maps.append(mp)

    # unmapped events
    for p in ecu.apps:
        for ev in p.type.events:
            if (p.name, ev.name) not in mapped:
                err(ew, "RTE event '%s' (%s) of runnable '%s' in SWC '%s' has no RteEventToTaskMapping (ECUC_Rte_09020) - every event must be "
                        "mapped to a task (or, for OperationInvokedEvents, to task=null)" % (ev.name, ev.kind, ev.runnable, p.name))
        for rn in p.type.runnables.values():
            if not any(e.runnable == rn.name for e in p.type.events):
                err(ew, "runnable '%s' of SWC '%s' is started by no RTE event" % (rn.name, p.name))

    # ---- build task contents ------------------------------------------------------------------------------
    for mp in ecu.maps:
        if mp.task is None:
            continue
        t = tasks[mp.task]
        key = (mp.runnable.name, mp.proto)
        ev_os = (mp.trigger or {}).get("osEvent")
        ent = None
        for e in t.entries:
            if e.proto == mp.proto and e.runnable.name == mp.runnable.name and e.os_event == ev_os:
                ent = e
        if ent is None:
            ent = Obj(proto=mp.proto, runnable=mp.runnable, position=mp.position, os_event=ev_os, events=[], maps=[], info=mp.info,
                      disabled=[])
            t.entries.append(ent)
        elif ent.position != mp.position:
            err(ew, "runnable %s is mapped to task %s at two positions (%s and %s) for the same OsEvent" % (mp.runnable.name, t.name, ent.position, mp.position))
        ent.events.append(mp.event)
        ent.maps.append(mp)
        if mp.event.kind == "TIMING":
            for (port, mode) in mp.event.disabled:
                if not ecu.mode_mgr or not any(c.req == (mp.proto, port) and c.prov == (ecu.mode_mgr.proto, ecu.mode_mgr.port) for c in sy.connectors):
                    err(ew, "TimingEvent %s: disabledInMode refers to port %s.%s which is not connected to the mode manager" % (mp.event.name, mp.proto, port))
                else:
                    if mode not in ecu.mode_mgr.group.modes:
                        err(ew, "TimingEvent %s: unknown mode %s" % (mp.event.name, mode))
                    ent.disabled.append(mode)
    for t in tasks.values():
        t.entries.sort(key=lambda e: (e.position if e.position is not None else 0, e.runnable.name))
        # basic tasks: all entries must share one activation source (otherwise the body cannot tell why it runs)
        if t.type == "BASIC" and len(t.entries) > 1:
            keys = set((e.maps[0].trigger or {}).get("alarm") or e.maps[0].event.name for e in t.entries)
            if len(keys) > 1:
                err(ew, "BASIC task %s hosts runnables with different activation sources %s: use an EXTENDED task with one OsEvent per "
                        "source" % (t.name, sorted(keys)))
        if t.type == "EXTENDED" and not t.autostart:
            err(ew, "OsTask %s is EXTENDED but not autostarted: an extended task must be running (parked in WaitEvent) before anyone can SetEvent it" % t.name)
        if t.type == "EXTENDED" and t.entries:
            used = set(e.os_event for e in t.entries)
            for e in t.events:
                if e not in used:
                    sys.stderr.write("WARNING: %s: OsEvent %s of task %s is not used by any RTE event\n" % (ew, e, t.name))

    # ---- SchM / BSW task bodies ---------------------------------------------------------------------------
    for st in ec.get("SchM", {}).get("tasks", []):
        if st["task"] not in tasks:
            err(ew, "SchM task '%s' is not an OsTask" % st["task"])
        else:
            tasks[st["task"]].bsw = list(st["order"])
    for t in tasks.values():
        if not t.entries and not t.bsw:
            err(ew, "OsTask %s has no body: map an RTE event or list BSW functions in SchM.tasks" % t.name)
        if t.entries and t.bsw:
            err(ew, "OsTask %s mixes RTE runnables and BSW functions; keep them in separate tasks" % t.name)
        if t.bsw and t.type != "BASIC":
            err(ew, "OsTask %s (BSW task) must be BASIC" % t.name)
        # BSW MainFunction period vs alarm period
        if any(f.startswith("Com_MainFunction") for f in t.bsw):
            for al in t.alarms:
                cyc = (al.get("autostart") or {}).get("cycle", 0)
                if cyc * tick_us != com["mainFunctionPeriodMs"] * 1000:
                    err(ew, "alarm %s (cycle %d ticks) activates %s which calls Com_MainFunction*: Com.mainFunctionPeriodMs=%d" % (
                        al["name"], cyc, t.name, com["mainFunctionPeriodMs"]))
        if t.type == "BASIC" and not t.alarms and not t.autostart and not any(
                mp.task == t.name and (mp.trigger or {}).get("type") in ("INTERNAL_WRITE", "COM_NOTIFICATION") for mp in ecu.maps):
            err(ew, "OsTask %s is never activated (no autostart, no alarm, no RTE trigger)" % t.name)

    # ---- tasks of each runnable (server runnables inherit their callers' tasks) -----------------------------
    callers = {}
    for (pn, rname), ri in ecu.runnable_info.items():
        for (port, op, srv) in ri.calls:
            if srv.kind == "RUNNABLE":
                callers.setdefault((srv.proto, srv.runnable.name), []).append((pn, rname))
    direct_task = {}
    for mp in ecu.maps:
        if mp.task:
            direct_task.setdefault((mp.proto, mp.runnable.name), set()).add(mp.task)

    def tasks_of(k, seen=()):
        r = set(direct_task.get(k, set()))
        for c in callers.get(k, []):
            if c not in seen:
                r |= tasks_of(c, seen + (k,))
        return r

    ecu.tasks_of = tasks_of

    # ---- exclusive areas ----------------------------------------------------------------------------------
    eas = OrderedDict()
    for ea in ec["Rte"].get("RteExclusiveArea", []):
        if ea["swc"] not in [p.type.name for p in ecu.apps]:
            err(ew, "RteExclusiveArea %s: SWC type '%s' is not on this ECU" % (ea["name"], ea["swc"]))
            continue
        if ea["implementation"] not in ("OS_RESOURCE", "OS_INTERRUPT_BLOCKING", "ALL_INTERRUPT_BLOCKING"):
            err(ew, "RteExclusiveArea %s: unsupported implementation '%s'" % (ea["name"], ea["implementation"]))
        eas[ea["name"]] = ea
    for p in ecu.apps:
        for a in p.type.eas:
            if a not in eas:
                err(ew, "ExclusiveArea '%s' of SWC %s has no RteExclusiveArea configuration" % (a, p.type.name))
    ecu.eas = eas
    res_cfg = OrderedDict((r["name"], r) for r in os_.get("OsResource", []))
    ecu.res_cfg = res_cfg
    ceilings = OrderedDict()
    for rname, r in res_cfg.items():
        acc = set(r.get("accessedBy", []))
        for a in acc:
            if a not in tasks:
                err(ew, "OsResource %s: accessedBy unknown task '%s'" % (rname, a))
        computed = set()
        for ea in eas.values():
            if ea.get("resource") == rname and ea["implementation"] == "OS_RESOURCE":
                for p in ecu.apps:
                    if p.type.name != ea["swc"]:
                        continue
                    for rn in p.type.runnables.values():
                        if ea["name"] in rn.eas:
                            computed |= tasks_of((p.name, rn.name))
        missing = computed - acc
        if missing:
            err(ew, "OsResource %s (ceiling): task(s) %s can reach Exclusive Area via server runnables but are missing from accessedBy %s - "
                    "the ceiling would be too low and the EA would not be mutually exclusive" % (rname, sorted(missing), sorted(acc)))
        both = [tasks[a].prio for a in (acc | computed) if a in tasks]
        ceilings[rname] = max(both) if both else 0
        r["_computedAccessors"] = sorted(computed)
    for n, ea in eas.items():
        if ea["implementation"] == "OS_RESOURCE" and ea.get("resource") not in res_cfg:
            err(ew, "RteExclusiveArea %s: resource '%s' is not an OsResource" % (n, ea.get("resource")))
    ecu.ceilings = ceilings

    # ---- Com notifications must have an RTE event ---------------------------------------------------------
    cbk_used = set()
    for mp in ecu.maps:
        if mp.trigger and mp.trigger.get("type") == "COM_NOTIFICATION":
            cbk_used.add("Rte_COMCbk_%s" % mp.trigger["comSignal"])
    for s in csigs.values():
        n = s.get("notification")
        if n and n not in cbk_used:
            err(ew, "ComSignal %s has notification %s but no DataReceivedEvent is mapped with trigger COM_NOTIFICATION for it" % (s["name"], n))

    # ---- API name collisions (single-instance RTE has no Rte_Instance parameter) --------------------------
    api_owner = {}

    def claim(name, owner):
        if name in api_owner and api_owner[name] != owner:
            err("RTE", "API name collision: %s is generated for both %s and %s (needs instance handles - not supported)" % (name, api_owner[name], owner))
        api_owner[name] = owner

    for (pn, rname), ri in ecu.runnable_info.items():
        for (port, elem, src) in ri.rd:
            claim("Rte_Read_%s_%s" % (port, elem), pn)
        for (port, elem, sk) in ri.wr:
            claim("Rte_Write_%s_%s" % (port, elem), pn)
        for (port, op, srv) in ri.calls:
            claim("Rte_Call_%s_%s" % (port, op.name), pn)
    if errs:
        raise GenError(errs)
    return ecu
