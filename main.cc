#include <queue>
#include <vector>
#include <fstream>
#include "ns3/gnuplot.h"
#include "ns3/event-id.h"
#include "ns3/udp-header.h"
#include "ns3/core-module.h"
#include "ns3/packet-sink.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;
using namespace std;

void ThroughputMonitor(FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> flowMon, Gnuplot2dDataset DataSet) {
    double localThrou = 0;
    map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier>(fmhelper->GetClassifier());
    for (map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin();
         stats != flowStats.end();
         ++stats) {
        Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow(stats->first);
        cout << "Flow ID			: "
             << stats->first << " ; " << fiveTuple.sourceAddress
             << " -----> " << fiveTuple.destinationAddress << endl;
        cout << "Tx Packets = " << stats->second.txPackets << endl;
        cout << "Rx Packets = " << stats->second.rxPackets << endl;
        cout << "Duration		: "
             << (stats->second.timeLastRxPacket.GetSeconds() - stats->second.timeFirstTxPacket.GetSeconds())
             << endl;
        cout << "Last Received Packet	: " << stats->second.timeLastRxPacket.GetSeconds()
             << " Seconds" << endl;
        cout << "Throughput: "
             << stats->second.rxBytes * 8.0 /
                         (stats->second.timeLastRxPacket.GetSeconds() -
                          stats->second.timeFirstTxPacket.GetSeconds()) /
                         1024 / 1024
                  << " Mbps" << endl;
        localThrou = stats->second.rxBytes * 8.0 /
                     (stats->second.timeLastRxPacket.GetSeconds() -
                      stats->second.timeFirstTxPacket.GetSeconds()) /
                     1024 / 1024;
        if (stats->first == 1) {
            DataSet.Add((double)Simulator::Now().GetSeconds(), (double)localThrou);
        }
        cout << "---------------------------------------------------------------------------" << endl;
    }
    Simulator::Schedule(Seconds(10), &ThroughputMonitor, fmhelper, flowMon, DataSet);
    flowMon->SerializeToXmlFile("ThroughputMonitor.xml", true, true);
}

int main(int argc, char* argv[]) {

	ns3::CommandLine cmd;
    cmd.Parse (argc, argv);

    int error_rate = 100000;
//    double band_width = 100.0;
    int senders_count = 3;
    int receivers_count = 3;
    int packet_size = 1472;
    string data_rate = "33Mb/s";
    string delay = "2ms";

    Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (packet_size));
    Config::SetDefault ("ns3::OnOffApplication::DataRate", DataRateValue (DataRate (data_rate)));

    NodeContainer network;
    network.Create(senders_count + receivers_count + 1);
    NodeContainer sender1 = NodeContainer(network.Get(0), network.Get(6));
    NodeContainer sender2 = NodeContainer(network.Get(1), network.Get(6));
    NodeContainer sender3 = NodeContainer(network.Get(2), network.Get(6));

    NodeContainer receiver1 = NodeContainer(network.Get(6), network.Get(3));
    NodeContainer receiver2 = NodeContainer(network.Get(6), network.Get(4));
    NodeContainer receiver3 = NodeContainer(network.Get(6), network.Get(5));

    InternetStackHelper internet;
    internet.Install(network);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(data_rate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer sender1_device = p2p.Install(sender1);
    NetDeviceContainer sender2_device = p2p.Install(sender2);
    NetDeviceContainer sender3_device = p2p.Install(sender3);

    NetDeviceContainer receiver1_device = p2p.Install(receiver1);
    NetDeviceContainer receiver2_device = p2p.Install(receiver2);
    NetDeviceContainer receiver3_device = p2p.Install(receiver3);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sender1_ip = ipv4.Assign(sender1_device);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer sender2_ip = ipv4.Assign(sender2_device);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer sender3_ip = ipv4.Assign(sender3_device);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer receiver1_ip = ipv4.Assign(receiver1_device);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer receiver2_ip = ipv4.Assign(receiver2_device);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer receiver3_ip = ipv4.Assign(receiver3_device);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t inboud_port = 1379;

    OnOffHelper onOffHelper_sender("ns3::UdpSocketFactory",Address(InetSocketAddress(sender1_ip.GetAddress(1), inboud_port)));
    onOffHelper_sender.SetConstantRate(DataRate(data_rate));
    ApplicationContainer application_sender = onOffHelper_sender.Install(network.Get(0));
    application_sender.Start(Seconds(0.1));
    application_sender.Stop(Seconds(15.0));

    onOffHelper_sender.SetAttribute("Remote", AddressValue(InetSocketAddress(sender2_ip.GetAddress(1), inboud_port)));
    application_sender = onOffHelper_sender.Install(network.Get(1));
    application_sender.Start(Seconds(0.2));
    application_sender.Stop(Seconds(15.0));

    onOffHelper_sender.SetAttribute("Remote", AddressValue(InetSocketAddress(sender3_ip.GetAddress(1), inboud_port)));
    application_sender = onOffHelper_sender.Install(network.Get(2));
    application_sender.Start(Seconds(0.3));
    application_sender.Stop(Seconds(15.0));

    uint16_t outbound_port = 1380;

    queue<uint32_t> inbound_data;
    for (int i = 0; i < packet_size; ++i) { inbound_data.push(i); }

    vector<uint32_t> outbound_data1;
    vector<uint32_t> outbound_data2;
    vector<uint32_t> outbound_data3;

    while (!inbound_data.empty()) {
        int error = rand() % error_rate;
        if (error == 0) {
            inbound_data.pop();
            continue;
        }

        int choice = (rand() % 3) + 1;
        switch (choice) {
        case 1:
            outbound_data1.push_back(inbound_data.front());
            break;
        case 2:
            outbound_data2.push_back(inbound_data.front());
            break;
        case 3:
            outbound_data3.push_back(inbound_data.front());
            break;
        }
        inbound_data.pop();
    }

    OnOffHelper onOffHelper_receiver("ns3::TcpSocketFactory", Address(InetSocketAddress(receiver1_ip.GetAddress(1), outbound_port)));
    onOffHelper_receiver.SetConstantRate(DataRate(data_rate));

    ApplicationContainer application_receiver = onOffHelper_receiver.Install(network.Get(6));
    application_receiver.Start(Seconds(1.0));
    application_receiver.Stop(Seconds(20));

    onOffHelper_receiver.SetAttribute("Remote", AddressValue(InetSocketAddress(receiver2_ip.GetAddress(1), outbound_port)));

    application_receiver = onOffHelper_receiver.Install(network.Get(6));
    application_receiver.Start(Seconds(1.0));
    application_receiver.Stop(Seconds(20));

    onOffHelper_receiver.SetAttribute("Remote", AddressValue(InetSocketAddress(receiver3_ip.GetAddress(1), outbound_port)));

    application_receiver = onOffHelper_receiver.Install(network.Get(6));
    application_receiver.Start(Seconds(1.0));
    application_receiver.Stop(Seconds(20));

    string fileNameWithNoExtension = "FlowVSThroughput_";
    string mainPlotTitle = "Flow vs Throughput";
    string graphicsFileName = fileNameWithNoExtension + ".png";
    string plotFileName = fileNameWithNoExtension + ".plt";
    string plotTitle = mainPlotTitle + ", Error: ";
    string dataTitle = "Throughput";

    Gnuplot gnuplot(graphicsFileName);
    gnuplot.SetTitle(plotTitle);

    gnuplot.SetTerminal("png");

    gnuplot.SetLegend("Flow", "Throughput");

    Gnuplot2dDataset dataset;
    dataset.SetTitle(dataTitle);
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    ThroughputMonitor(&flowHelper, flowMonitor, dataset);

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("out.tr"));
    p2p.EnablePcapAll("out");

    Simulator::Stop(Seconds(20));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
