#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/enum.h"
#include "ns3/error-model.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-header.h"
#include "ns3/traffic-control-module.h"
#include "ns3/udp-header.h"

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/tcp-westwood.h"

#include <fstream>
#include <string>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("LoadBalancer");

void
ThroughputMonitor(FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> flowMon, Gnuplot2dDataset DataSet)
{
    double localThrou = 0;
    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier>(fmhelper->GetClassifier());
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin();
         stats != flowStats.end();
         ++stats)
    {
        Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow(stats->first);
        std::cout << "Flow ID			: " << stats->first << " ; " << fiveTuple.sourceAddress
                  << " -----> " << fiveTuple.destinationAddress << std::endl;
        std::cout << "Tx Packets = " << stats->second.txPackets << std::endl;
        std::cout << "Rx Packets = " << stats->second.rxPackets << std::endl;
        std::cout << "Duration		: "
                  << (stats->second.timeLastRxPacket.GetSeconds() -
                      stats->second.timeFirstTxPacket.GetSeconds())
                  << std::endl;
        std::cout << "Last Received Packet	: " << stats->second.timeLastRxPacket.GetSeconds()
                  << " Seconds" << std::endl;
        std::cout << "Throughput: "
                  << stats->second.rxBytes * 8.0 /
                         (stats->second.timeLastRxPacket.GetSeconds() -
                          stats->second.timeFirstTxPacket.GetSeconds()) /
                         1024 / 1024
                  << " Mbps" << std::endl;
        localThrou = stats->second.rxBytes * 8.0 /
                     (stats->second.timeLastRxPacket.GetSeconds() -
                      stats->second.timeFirstTxPacket.GetSeconds()) /
                     1024 / 1024;
        if (stats->first == 1)
        {
            DataSet.Add((double)Simulator::Now().GetSeconds(), (double)localThrou);
        }
        std::cout << "---------------------------------------------------------------------------"
                  << std::endl;
    }
    Simulator::Schedule(Seconds(10), &ThroughputMonitor, fmhelper, flowMon, DataSet);
    flowMon->SerializeToXmlFile("ThroughputMonitor.xml", true, true);
}

int main(int argc, char *argv[]) {

    uint32_t payloadSize = 1472;
    std::string dataRate = "100Mbps";
    std::string errorRate = "0.001";
    std::string tcpVariant = "TcpNewReno";
    std::string phyRate = "HtMcs7";
    double simulationTime = 10;
    bool pcapTracing = false;

    /* Command line argument parser setup. */
    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.AddValue("errorRate", "Application error rate", errorRate);
    cmd.AddValue("tcpVariant",
                 "Transport protocol to use: TcpNewReno, "
                 "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                 "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ",
                 tcpVariant);
    cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("pcap", "Enable/disable PCAP Tracing", pcapTracing);
    cmd.Parse(argc, argv);

    tcpVariant = std::string("ns3::") + tcpVariant;
    // Select TCP variant
    if (tcpVariant == "ns3::TcpWestwoodPlus")
    {
        // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
        // the default protocol type in ns3::TcpWestwood is WESTWOOD
        Config::SetDefault("ns3::TcpWestwood::ProtocolType", EnumValue(TcpWestwood::WESTWOODPLUS));
    }
    else
    {
        TypeId tcpTid;
        NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid),
                            "TypeId " << tcpVariant << " not found");
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           TypeIdValue(TypeId::LookupByName(tcpVariant)));
    }

    /* Configure TCP Options */
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));

    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(ns3::WIFI_STANDARD_UNSPECIFIED);

    /* Set up Legacy Channel */
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5e9));

    /* Setup Physical Layer */
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode",
                                       StringValue(phyRate),
                                       "ControlMode",
                                       StringValue("HtMcs0"));

    NodeContainer networkNodes;
    networkNodes.Create(7);

    Ptr<Node> sender1 = networkNodes.Get(0);
    Ptr<Node> sender2 = networkNodes.Get(1);
    Ptr<Node> sender3 = networkNodes.Get(2);

    Ptr<Node> receiver1 = networkNodes.Get(3);
    Ptr<Node> receiver2 = networkNodes.Get(4);
    Ptr<Node> receiver3 = networkNodes.Get(5);

    Ptr<Node> load_balancer = networkNodes.Get(6);


    /* Configure AP */
    Ssid ssid = Ssid("network");
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifiHelper.Install(wifiPhy, wifiMac, load_balancer);

    /* Configure STA */
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices;
    staDevices = wifiHelper.Install(wifiPhy, wifiMac, sender1);
    staDevices = wifiHelper.Install(wifiPhy, wifiMac, sender2);
    staDevices = wifiHelper.Install(wifiPhy, wifiMac, sender3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper address;

    NetDeviceContainer load_balancer_devices = pointToPoint.Install(load_balancer);

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(load_balancer_devices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    address.Assign(staDevices);
    address.Assign(load_balancer_devices);

    UdpEchoServerHelper echoServer(9);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}