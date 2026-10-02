# AUTOSAR Classic Platform 规范（R25-11）——按 Stack 分类

> 来源：[autosar.org](https://www.autosar.org/standards/classic-platform) 官方下载区 `fileadmin/standards/R25-11/CP/`。版权归 AUTOSAR 所有，使用受 [AUTOSAR Terms of Use](https://www.autosar.org/fileadmin/user_upload/Documents/AUTOSAR_Terms_Of_Use.pdf) 约束。
> **PDF 不提交到 git**（`.gitignore` 中的 `*.pdf`）。在本地重新下载：`python tools/download_autosar_specs.py`（可加 `--release R24-11` 下载其他版本，脚本会跳过已存在的文件）。

> 共 **105** 份文档，约 **171 MB**。范围：所有 BSW/MCAL 模块的 SWS + 通用基础文档；未包含 RS（需求）文档、V2X、Application Interfaces 等与本教程无关的部分。

## 与教程页码的关系（重要）

教程中引用的 SWS 页码来自仓库根目录的**旧版本** PDF：DCM **R20-11**、CAN Driver **R22-11**、MCU / IoHwAb **R24-11**。本目录是 **R25-11**，同一条 `SWS_xxx` requirement ID 通常保留，但页码会变，个别 API/配置在版本间有改动（这正是 DCM 升级时要比对的内容，见 [dcm-upgrade-guide.md](../../docs/dcm-upgrade-guide.md)）。真实 RTA-CAR 项目对应哪个 release，需在项目中确认后再下载对应版本。

本仓库先前缺失的 CanIf / CanTp / PduR / Dem / NvM / Rte / Os / EcuM / BswM / ComM 等 SWS 现在都在下面，教程中标注“本仓库无该 SWS，需确认”的地方可以用这些文件核对（注意 release 差异）。

建议阅读顺序：`00-general/AUTOSAR_CP_EXP_LayeredSoftwareArchitecture.pdf` → `01`/`02` MCAL → `04` CAN Driver → `06` CAN Stack → `11` PduR → `12` Dcm/Dem → `16` Rte。

## 00-general — 通用 / 基础类型 / 架构（教程 Part II）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_EXP_LayeredSoftwareArchitecture.pdf](00-general/AUTOSAR_CP_EXP_LayeredSoftwareArchitecture.pdf) | 分层架构总览（建议第一篇读） | 2.6 MB |
| [AUTOSAR_CP_RS_BSWGeneral.pdf](00-general/AUTOSAR_CP_RS_BSWGeneral.pdf) | BSW 通用需求 | 0.4 MB |
| [AUTOSAR_CP_SWS_BSWGeneral.pdf](00-general/AUTOSAR_CP_SWS_BSWGeneral.pdf) | BSW 通用要求（含 Compiler/MemMap 约定） | 0.4 MB |
| [AUTOSAR_CP_SWS_CRCLibrary.pdf](00-general/AUTOSAR_CP_SWS_CRCLibrary.pdf) | Crc | 0.4 MB |
| [AUTOSAR_CP_SWS_CommunicationStackTypes.pdf](00-general/AUTOSAR_CP_SWS_CommunicationStackTypes.pdf) | ComStack_Types | 0.1 MB |
| [AUTOSAR_CP_SWS_E2ELibrary.pdf](00-general/AUTOSAR_CP_SWS_E2ELibrary.pdf) | E2E | 1.8 MB |
| [AUTOSAR_CP_SWS_MemoryMapping.pdf](00-general/AUTOSAR_CP_SWS_MemoryMapping.pdf) | MemMap | 0.5 MB |
| [AUTOSAR_CP_SWS_PlatformTypes.pdf](00-general/AUTOSAR_CP_SWS_PlatformTypes.pdf) | Platform_Types | 0.2 MB |
| [AUTOSAR_CP_SWS_StandardTypes.pdf](00-general/AUTOSAR_CP_SWS_StandardTypes.pdf) | Std_Types | 0.1 MB |
| [AUTOSAR_CP_TPS_ECUConfiguration.pdf](00-general/AUTOSAR_CP_TPS_ECUConfiguration.pdf) | ECU Configuration 方法（ARXML/ECUC） | 2.6 MB |
| [AUTOSAR_CP_TR_VFB.pdf](00-general/AUTOSAR_CP_TR_VFB.pdf) | Virtual Functional Bus | 7.0 MB |

## 01-mcal-microcontroller — MCAL — Microcontroller Drivers（教程 Part III）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_CoreTest.pdf](01-mcal-microcontroller/AUTOSAR_CP_SWS_CoreTest.pdf) | CorTst | 0.3 MB |
| [AUTOSAR_CP_SWS_FlashTest.pdf](01-mcal-microcontroller/AUTOSAR_CP_SWS_FlashTest.pdf) | FlsTst | 0.4 MB |
| [AUTOSAR_CP_SWS_GPTDriver.pdf](01-mcal-microcontroller/AUTOSAR_CP_SWS_GPTDriver.pdf) | Gpt | 0.4 MB |
| [AUTOSAR_CP_SWS_MCUDriver.pdf](01-mcal-microcontroller/AUTOSAR_CP_SWS_MCUDriver.pdf) | Mcu | 0.4 MB |
| [AUTOSAR_CP_SWS_RAMTest.pdf](01-mcal-microcontroller/AUTOSAR_CP_SWS_RAMTest.pdf) | RamTst | 0.6 MB |
| [AUTOSAR_CP_SWS_WatchdogDriver.pdf](01-mcal-microcontroller/AUTOSAR_CP_SWS_WatchdogDriver.pdf) | Wdg | 0.2 MB |

## 02-mcal-io — MCAL — I/O Drivers（教程 Part III）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_ADCDriver.pdf](02-mcal-io/AUTOSAR_CP_SWS_ADCDriver.pdf) | Adc | 0.7 MB |
| [AUTOSAR_CP_SWS_DIODriver.pdf](02-mcal-io/AUTOSAR_CP_SWS_DIODriver.pdf) | Dio | 0.3 MB |
| [AUTOSAR_CP_SWS_ICUDriver.pdf](02-mcal-io/AUTOSAR_CP_SWS_ICUDriver.pdf) | Icu | 0.8 MB |
| [AUTOSAR_CP_SWS_OCUDriver.pdf](02-mcal-io/AUTOSAR_CP_SWS_OCUDriver.pdf) | Ocu | 1.1 MB |
| [AUTOSAR_CP_SWS_PWMDriver.pdf](02-mcal-io/AUTOSAR_CP_SWS_PWMDriver.pdf) | Pwm | 0.4 MB |
| [AUTOSAR_CP_SWS_PortDriver.pdf](02-mcal-io/AUTOSAR_CP_SWS_PortDriver.pdf) | Port | 0.3 MB |

## 03-mcal-memory — MCAL — Memory Drivers

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_MemoryAccess.pdf](03-mcal-memory/AUTOSAR_CP_SWS_MemoryAccess.pdf) | MemAcc | 0.4 MB |
| [AUTOSAR_CP_SWS_MemoryDriver.pdf](03-mcal-memory/AUTOSAR_CP_SWS_MemoryDriver.pdf) | Mem (取代 Fls/Eep) | 0.3 MB |

## 04-mcal-communication-drivers — MCAL — Communication Drivers（教程 Part IV）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_CANDriver.pdf](04-mcal-communication-drivers/AUTOSAR_CP_SWS_CANDriver.pdf) | Can | 1.1 MB |
| [AUTOSAR_CP_SWS_CANXLDriver.pdf](04-mcal-communication-drivers/AUTOSAR_CP_SWS_CANXLDriver.pdf) | CanXL | 0.4 MB |
| [AUTOSAR_CP_SWS_EthernetDriver.pdf](04-mcal-communication-drivers/AUTOSAR_CP_SWS_EthernetDriver.pdf) | Eth | 0.9 MB |
| [AUTOSAR_CP_SWS_FlexRayDriver.pdf](04-mcal-communication-drivers/AUTOSAR_CP_SWS_FlexRayDriver.pdf) | Fr | 0.6 MB |
| [AUTOSAR_CP_SWS_I2CDriver.pdf](04-mcal-communication-drivers/AUTOSAR_CP_SWS_I2CDriver.pdf) | I2c | 0.7 MB |
| [AUTOSAR_CP_SWS_LINDriver.pdf](04-mcal-communication-drivers/AUTOSAR_CP_SWS_LINDriver.pdf) | Lin | 0.3 MB |
| [AUTOSAR_CP_SWS_SPIHandlerDriver.pdf](04-mcal-communication-drivers/AUTOSAR_CP_SWS_SPIHandlerDriver.pdf) | Spi | 12.0 MB |
| [AUTOSAR_CP_SWS_WirelessEthernetDriver.pdf](04-mcal-communication-drivers/AUTOSAR_CP_SWS_WirelessEthernetDriver.pdf) | WEth | 0.4 MB |

## 05-io-hw-abstraction — ECU Abstraction — I/O HW Abstraction（教程 Part III）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_IOHardwareAbstraction.pdf](05-io-hw-abstraction/AUTOSAR_CP_SWS_IOHardwareAbstraction.pdf) | IoHwAb | 6.9 MB |
| [AUTOSAR_CP_SWS_WatchdogInterface.pdf](05-io-hw-abstraction/AUTOSAR_CP_SWS_WatchdogInterface.pdf) | WdgIf | 0.2 MB |

## 06-can-stack — CAN Stack（本教程主线）（教程 Part IV–V）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_CANInterface.pdf](06-can-stack/AUTOSAR_CP_SWS_CANInterface.pdf) | CanIf | 1.5 MB |
| [AUTOSAR_CP_SWS_CANNetworkManagement.pdf](06-can-stack/AUTOSAR_CP_SWS_CANNetworkManagement.pdf) | CanNm | 0.6 MB |
| [AUTOSAR_CP_SWS_CANStateManager.pdf](06-can-stack/AUTOSAR_CP_SWS_CANStateManager.pdf) | CanSM | 0.6 MB |
| [AUTOSAR_CP_SWS_CANTransceiverDriver.pdf](06-can-stack/AUTOSAR_CP_SWS_CANTransceiverDriver.pdf) | CanTrcv | 0.6 MB |
| [AUTOSAR_CP_SWS_CANTransportLayer.pdf](06-can-stack/AUTOSAR_CP_SWS_CANTransportLayer.pdf) | CanTp | 1.6 MB |
| [AUTOSAR_CP_SWS_CANXLTransceiverDriver.pdf](06-can-stack/AUTOSAR_CP_SWS_CANXLTransceiverDriver.pdf) | CanXLTrcv | 0.2 MB |
| [AUTOSAR_CP_SWS_TimeSyncOverCAN.pdf](06-can-stack/AUTOSAR_CP_SWS_TimeSyncOverCAN.pdf) | CanTSyn | 0.7 MB |

## 07-lin-stack — LIN Stack

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_LINInterface.pdf](07-lin-stack/AUTOSAR_CP_SWS_LINInterface.pdf) | LinIf (含 LinTp) | 0.9 MB |
| [AUTOSAR_CP_SWS_LINStateManager.pdf](07-lin-stack/AUTOSAR_CP_SWS_LINStateManager.pdf) | LinSM | 3.0 MB |
| [AUTOSAR_CP_SWS_LINTransceiverDriver.pdf](07-lin-stack/AUTOSAR_CP_SWS_LINTransceiverDriver.pdf) | LinTrcv | 0.3 MB |

## 08-flexray-stack — FlexRay Stack

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_FlexRayARTransportLayer.pdf](08-flexray-stack/AUTOSAR_CP_SWS_FlexRayARTransportLayer.pdf) | FrArTp | 2.2 MB |
| [AUTOSAR_CP_SWS_FlexRayISOTransportLayer.pdf](08-flexray-stack/AUTOSAR_CP_SWS_FlexRayISOTransportLayer.pdf) | FrTp | 2.6 MB |
| [AUTOSAR_CP_SWS_FlexRayInterface.pdf](08-flexray-stack/AUTOSAR_CP_SWS_FlexRayInterface.pdf) | FrIf | 0.9 MB |
| [AUTOSAR_CP_SWS_FlexRayNetworkManagement.pdf](08-flexray-stack/AUTOSAR_CP_SWS_FlexRayNetworkManagement.pdf) | FrNm | 1.0 MB |
| [AUTOSAR_CP_SWS_FlexRayStateManager.pdf](08-flexray-stack/AUTOSAR_CP_SWS_FlexRayStateManager.pdf) | FrSM | 0.5 MB |
| [AUTOSAR_CP_SWS_FlexRayTransceiverDriver.pdf](08-flexray-stack/AUTOSAR_CP_SWS_FlexRayTransceiverDriver.pdf) | FrTrcv | 0.6 MB |
| [AUTOSAR_CP_SWS_TimeSyncOverFlexRay.pdf](08-flexray-stack/AUTOSAR_CP_SWS_TimeSyncOverFlexRay.pdf) | FrTSyn | 0.7 MB |

## 09-ethernet-stack — Ethernet / IP Stack

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_DiagnosticOverIP.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_DiagnosticOverIP.pdf) | DoIP | 0.8 MB |
| [AUTOSAR_CP_SWS_EthernetInterface.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_EthernetInterface.pdf) | EthIf | 1.3 MB |
| [AUTOSAR_CP_SWS_EthernetStateManager.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_EthernetStateManager.pdf) | EthSM | 0.5 MB |
| [AUTOSAR_CP_SWS_EthernetSwitchDriver.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_EthernetSwitchDriver.pdf) | EthSwt | 2.1 MB |
| [AUTOSAR_CP_SWS_EthernetTransceiverDriver.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_EthernetTransceiverDriver.pdf) | EthTrcv | 0.6 MB |
| [AUTOSAR_CP_SWS_SOMEIPTransportProtocol.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_SOMEIPTransportProtocol.pdf) | SomeIpTp | 0.4 MB |
| [AUTOSAR_CP_SWS_ServiceDiscovery.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_ServiceDiscovery.pdf) | Sd | 2.6 MB |
| [AUTOSAR_CP_SWS_SocketAdaptor.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_SocketAdaptor.pdf) | SoAd | 1.9 MB |
| [AUTOSAR_CP_SWS_TcpIp.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_TcpIp.pdf) | TcpIp | 1.8 MB |
| [AUTOSAR_CP_SWS_TimeSyncOverEthernet.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_TimeSyncOverEthernet.pdf) | EthTSyn | 0.9 MB |
| [AUTOSAR_CP_SWS_UDPNetworkManagement.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_UDPNetworkManagement.pdf) | UdpNm | 0.6 MB |
| [AUTOSAR_CP_SWS_WirelessEthernetTransceiverDriver.pdf](09-ethernet-stack/AUTOSAR_CP_SWS_WirelessEthernetTransceiverDriver.pdf) | WEthTrcv | 0.3 MB |

## 10-j1939-stack — SAE J1939

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_SAEJ1939DiagnosticCommunicationManager.pdf](10-j1939-stack/AUTOSAR_CP_SWS_SAEJ1939DiagnosticCommunicationManager.pdf) | J1939Dcm | 0.6 MB |
| [AUTOSAR_CP_SWS_SAEJ1939FunctionalSafetyCommProtocol.pdf](10-j1939-stack/AUTOSAR_CP_SWS_SAEJ1939FunctionalSafetyCommProtocol.pdf) | J1939FSCP | 0.9 MB |
| [AUTOSAR_CP_SWS_SAEJ1939NetworkManagement.pdf](10-j1939-stack/AUTOSAR_CP_SWS_SAEJ1939NetworkManagement.pdf) | J1939Nm | 0.4 MB |
| [AUTOSAR_CP_SWS_SAEJ1939RequestManager.pdf](10-j1939-stack/AUTOSAR_CP_SWS_SAEJ1939RequestManager.pdf) | J1939Rm | 0.5 MB |
| [AUTOSAR_CP_SWS_SAEJ1939TransportLayer.pdf](10-j1939-stack/AUTOSAR_CP_SWS_SAEJ1939TransportLayer.pdf) | J1939Tp | 0.6 MB |

## 11-com-services — Communication Services（Com / PduR / ComM / SecOC …）（教程 Part V）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_BusMirroring.pdf](11-com-services/AUTOSAR_CP_SWS_BusMirroring.pdf) | Mirror | 0.8 MB |
| [AUTOSAR_CP_SWS_COM.pdf](11-com-services/AUTOSAR_CP_SWS_COM.pdf) | Com | 2.9 MB |
| [AUTOSAR_CP_SWS_COMBasedTransformer.pdf](11-com-services/AUTOSAR_CP_SWS_COMBasedTransformer.pdf) | ComXf | 0.4 MB |
| [AUTOSAR_CP_SWS_COMManager.pdf](11-com-services/AUTOSAR_CP_SWS_COMManager.pdf) | ComM | 1.5 MB |
| [AUTOSAR_CP_SWS_DiagnosticLogAndTrace.pdf](11-com-services/AUTOSAR_CP_SWS_DiagnosticLogAndTrace.pdf) | Dlt | 0.9 MB |
| [AUTOSAR_CP_SWS_E2ETransformer.pdf](11-com-services/AUTOSAR_CP_SWS_E2ETransformer.pdf) | E2EXf | 1.3 MB |
| [AUTOSAR_CP_SWS_IPDUMultiplexer.pdf](11-com-services/AUTOSAR_CP_SWS_IPDUMultiplexer.pdf) | IpduM | 1.0 MB |
| [AUTOSAR_CP_SWS_LSduRouter.pdf](11-com-services/AUTOSAR_CP_SWS_LSduRouter.pdf) | LSduR | 0.3 MB |
| [AUTOSAR_CP_SWS_NetworkManagementInterface.pdf](11-com-services/AUTOSAR_CP_SWS_NetworkManagementInterface.pdf) | Nm | 0.7 MB |
| [AUTOSAR_CP_SWS_PDURouter.pdf](11-com-services/AUTOSAR_CP_SWS_PDURouter.pdf) | PduR | 0.8 MB |
| [AUTOSAR_CP_SWS_SOMEIPTransformer.pdf](11-com-services/AUTOSAR_CP_SWS_SOMEIPTransformer.pdf) | SomeIpXf | 1.8 MB |
| [AUTOSAR_CP_SWS_SecureOnboardCommunication.pdf](11-com-services/AUTOSAR_CP_SWS_SecureOnboardCommunication.pdf) | SecOC | 8.9 MB |
| [AUTOSAR_CP_SWS_XCP.pdf](11-com-services/AUTOSAR_CP_SWS_XCP.pdf) | Xcp | 0.4 MB |

## 12-diagnostics — Diagnostics（DCM / DEM / FiM / DET）（教程 Part VI）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_DefaultErrorTracer.pdf](12-diagnostics/AUTOSAR_CP_SWS_DefaultErrorTracer.pdf) | Det | 0.2 MB |
| [AUTOSAR_CP_SWS_DiagnosticCommunicationManager.pdf](12-diagnostics/AUTOSAR_CP_SWS_DiagnosticCommunicationManager.pdf) | Dcm | 5.1 MB |
| [AUTOSAR_CP_SWS_DiagnosticEventManager.pdf](12-diagnostics/AUTOSAR_CP_SWS_DiagnosticEventManager.pdf) | Dem | 7.1 MB |
| [AUTOSAR_CP_SWS_FunctionInhibitionManager.pdf](12-diagnostics/AUTOSAR_CP_SWS_FunctionInhibitionManager.pdf) | FiM | 0.4 MB |

## 13-memory-services — Memory Services（NvM / MemIf / Fee / Ea）（教程 Part VI（NvM））

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_EXP_NVDataHandling.pdf](13-memory-services/AUTOSAR_CP_EXP_NVDataHandling.pdf) | NV 数据处理说明 | 1.1 MB |
| [AUTOSAR_CP_SWS_BulkNvDataManager.pdf](13-memory-services/AUTOSAR_CP_SWS_BulkNvDataManager.pdf) | BndM | 0.2 MB |
| [AUTOSAR_CP_SWS_EEPROMAbstraction.pdf](13-memory-services/AUTOSAR_CP_SWS_EEPROMAbstraction.pdf) | Ea | 0.5 MB |
| [AUTOSAR_CP_SWS_FlashEEPROMEmulation.pdf](13-memory-services/AUTOSAR_CP_SWS_FlashEEPROMEmulation.pdf) | Fee | 0.4 MB |
| [AUTOSAR_CP_SWS_MemoryAbstractionInterface.pdf](13-memory-services/AUTOSAR_CP_SWS_MemoryAbstractionInterface.pdf) | MemIf | 0.2 MB |
| [AUTOSAR_CP_SWS_NVRAMManager.pdf](13-memory-services/AUTOSAR_CP_SWS_NVRAMManager.pdf) | NvM | 5.6 MB |

## 14-system-services — System Services（OS / EcuM / BswM / WdgM / StbM）（教程 Part II / X）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_BSWModeManager.pdf](14-system-services/AUTOSAR_CP_SWS_BSWModeManager.pdf) | BswM | 1.3 MB |
| [AUTOSAR_CP_SWS_ECUStateManager.pdf](14-system-services/AUTOSAR_CP_SWS_ECUStateManager.pdf) | EcuM | 1.6 MB |
| [AUTOSAR_CP_SWS_HWTestManager.pdf](14-system-services/AUTOSAR_CP_SWS_HWTestManager.pdf) | HTMSS | 17.9 MB |
| [AUTOSAR_CP_SWS_OS.pdf](14-system-services/AUTOSAR_CP_SWS_OS.pdf) | Os | 4.8 MB |
| [AUTOSAR_CP_SWS_SynchronizedTimeBaseManager.pdf](14-system-services/AUTOSAR_CP_SWS_SynchronizedTimeBaseManager.pdf) | StbM | 1.5 MB |
| [AUTOSAR_CP_SWS_TimeService.pdf](14-system-services/AUTOSAR_CP_SWS_TimeService.pdf) | Tm | 0.5 MB |
| [AUTOSAR_CP_SWS_WatchdogManager.pdf](14-system-services/AUTOSAR_CP_SWS_WatchdogManager.pdf) | WdgM | 0.9 MB |

## 15-crypto-security — Crypto / Security

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_CryptoDriver.pdf](15-crypto-security/AUTOSAR_CP_SWS_CryptoDriver.pdf) | Crypto | 0.7 MB |
| [AUTOSAR_CP_SWS_CryptoInterface.pdf](15-crypto-security/AUTOSAR_CP_SWS_CryptoInterface.pdf) | CryIf | 0.3 MB |
| [AUTOSAR_CP_SWS_CryptoServiceManager.pdf](15-crypto-security/AUTOSAR_CP_SWS_CryptoServiceManager.pdf) | Csm | 4.1 MB |
| [AUTOSAR_CP_SWS_IntrusionDetectionSystemManager.pdf](15-crypto-security/AUTOSAR_CP_SWS_IntrusionDetectionSystemManager.pdf) | IdsM | 1.0 MB |
| [AUTOSAR_CP_SWS_KeyManager.pdf](15-crypto-security/AUTOSAR_CP_SWS_KeyManager.pdf) | KeyM | 1.2 MB |

## 16-rte — RTE（教程 Part VII）

| 文件 | 模块 | 大小 |
|---|---|---|
| [AUTOSAR_CP_SWS_RTE.pdf](16-rte/AUTOSAR_CP_SWS_RTE.pdf) | Rte / SchM | 10.9 MB |
