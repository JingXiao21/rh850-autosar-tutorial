"""Download AUTOSAR Classic Platform specifications, grouped by stack.

The PDFs are (c) AUTOSAR and are NOT committed (see .gitignore); this script
re-creates the local copy from the official autosar.org download area.

Usage:  python tools/download_autosar_specs.py [--release R25-11] [--dest specs/autosar-cp-R25-11]
"""
import argparse, os, sys, urllib.request, concurrent.futures as cf

GROUPS=[
 ("00-general",["EXP_LayeredSoftwareArchitecture","TR_VFB","RS_BSWGeneral","SWS_BSWGeneral","SWS_StandardTypes","SWS_PlatformTypes","SWS_MemoryMapping","SWS_CommunicationStackTypes","TPS_ECUConfiguration","SWS_CRCLibrary","SWS_E2ELibrary","TR_Methodology","TR_SWCModelingGuide","TPS_SoftwareComponentTemplate","TPS_SystemTemplate"]),
 ("01-mcal-microcontroller",["SWS_MCUDriver","SWS_GPTDriver","SWS_WatchdogDriver","SWS_CoreTest","SWS_RAMTest","SWS_FlashTest"]),
 ("02-mcal-io",["SWS_PortDriver","SWS_DIODriver","SWS_ADCDriver","SWS_PWMDriver","SWS_ICUDriver","SWS_OCUDriver"]),
 ("03-mcal-memory",["SWS_MemoryDriver","SWS_MemoryAccess"]),
 ("04-mcal-communication-drivers",["SWS_CANDriver","SWS_CANXLDriver","SWS_LINDriver","SWS_FlexRayDriver","SWS_EthernetDriver","SWS_WirelessEthernetDriver","SWS_SPIHandlerDriver","SWS_I2CDriver"]),
 ("05-io-hw-abstraction",["SWS_IOHardwareAbstraction","SWS_WatchdogInterface"]),
 ("06-can-stack",["SWS_CANInterface","SWS_CANTransceiverDriver","SWS_CANXLTransceiverDriver","SWS_CANTransportLayer","SWS_CANStateManager","SWS_CANNetworkManagement","SWS_TimeSyncOverCAN"]),
 ("07-lin-stack",["SWS_LINInterface","SWS_LINTransceiverDriver","SWS_LINStateManager"]),
 ("08-flexray-stack",["SWS_FlexRayInterface","SWS_FlexRayTransceiverDriver","SWS_FlexRayStateManager","SWS_FlexRayNetworkManagement","SWS_FlexRayISOTransportLayer","SWS_FlexRayARTransportLayer","SWS_TimeSyncOverFlexRay"]),
 ("09-ethernet-stack",["SWS_EthernetInterface","SWS_EthernetTransceiverDriver","SWS_WirelessEthernetTransceiverDriver","SWS_EthernetSwitchDriver","SWS_EthernetStateManager","SWS_TcpIp","SWS_SocketAdaptor","SWS_ServiceDiscovery","SWS_UDPNetworkManagement","SWS_SOMEIPTransportProtocol","SWS_DiagnosticOverIP","SWS_TimeSyncOverEthernet"]),
 ("10-j1939-stack",["SWS_SAEJ1939TransportLayer","SWS_SAEJ1939NetworkManagement","SWS_SAEJ1939RequestManager","SWS_SAEJ1939DiagnosticCommunicationManager","SWS_SAEJ1939FunctionalSafetyCommProtocol"]),
 ("11-com-services",["SWS_COM","SWS_PDURouter","SWS_IPDUMultiplexer","SWS_LSduRouter","SWS_COMManager","SWS_NetworkManagementInterface","SWS_SecureOnboardCommunication","SWS_COMBasedTransformer","SWS_SOMEIPTransformer","SWS_E2ETransformer","SWS_BusMirroring","SWS_XCP","SWS_DiagnosticLogAndTrace"]),
 ("12-diagnostics",["SWS_DiagnosticCommunicationManager","SWS_DiagnosticEventManager","SWS_FunctionInhibitionManager","SWS_DefaultErrorTracer"]),
 ("13-memory-services",["SWS_NVRAMManager","SWS_MemoryAbstractionInterface","SWS_FlashEEPROMEmulation","SWS_EEPROMAbstraction","SWS_BulkNvDataManager","EXP_NVDataHandling"]),
 ("14-system-services",["SWS_OS","SWS_ECUStateManager","SWS_BSWModeManager","SWS_WatchdogManager","SWS_SynchronizedTimeBaseManager","SWS_TimeService","SWS_HWTestManager"]),
 ("15-crypto-security",["SWS_CryptoDriver","SWS_CryptoInterface","SWS_CryptoServiceManager","SWS_KeyManager","SWS_IntrusionDetectionSystemManager"]),
 ("16-rte",["SWS_RTE"]),
]


def fetch(base, dest, group, name):
    os.makedirs(os.path.join(dest, group), exist_ok=True)
    path = os.path.join(dest, group, f"AUTOSAR_CP_{name}.pdf")
    if os.path.exists(path) and os.path.getsize(path) > 10000:
        return group, name, "exists", os.path.getsize(path)
    try:
        req = urllib.request.Request(base + name + ".pdf", headers={"User-Agent": "Mozilla/5.0"})
        data = urllib.request.urlopen(req, timeout=180).read()
    except Exception as exc:  # 404 when a document does not exist in this release
        return group, name, f"ERR {exc}", 0
    if not data.startswith(b"%PDF"):
        return group, name, "not a PDF", 0
    with open(path, "wb") as f:
        f.write(data)
    return group, name, "ok", len(data)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--release", default="R25-11")
    ap.add_argument("--dest", default=None)
    args = ap.parse_args()
    dest = args.dest or f"specs/autosar-cp-{args.release}"
    base = f"https://www.autosar.org/fileadmin/standards/{args.release}/CP/AUTOSAR_CP_"
    jobs = [(g, n) for g, names in GROUPS for n in names]
    with cf.ThreadPoolExecutor(8) as ex:
        results = list(ex.map(lambda j: fetch(base, dest, *j), jobs))
    for g, n, status, _ in results:
        if status not in ("ok", "exists"):
            print(f"MISSING {g}/{n}: {status}")
    ok = sum(r[2] in ("ok", "exists") for r in results)
    print(f"{ok}/{len(results)} documents in {dest} ({sum(r[3] for r in results) / 1e6:.1f} MB)")
    return 0 if ok == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
