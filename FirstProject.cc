    #include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/wave-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstScriptExample");

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(10);
  NS_LOG_INFO("WiFi Container Created");

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> wifiChannel = wifiChannelHelper.Create();
  NS_LOG_INFO("WiFi Channel configured");

  YansWifiPhyHelper wifiPhyLayer;
  wifiPhyLayer.SetChannel(wifiChannel);
  NS_LOG_INFO("Physical layer of the node is defined");

  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  wifi80211p.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                     "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer netDeviceContainer = wifi80211p.Install(wifiPhyLayer, wifi80211pMac, nodes);
  wifiPhyLayer.EnablePcap("animacao", netDeviceContainer);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(netDeviceContainer);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  NodeContainer clientNodes;
  for (uint32_t i = 1; i < 9; ++i) {
    clientNodes.Add(nodes.Get(i));
  }

  UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(clientNodes);
  clientApps.Start(Seconds(2.0));

  
  // Rastreando eventos de pacotes
  wifiPhyLayer.EnablePcap("animacao", netDeviceContainer);


  AnimationInterface anim("scratch/animacao.xml");
  anim.EnablePacketMetadata(true);
  anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
  anim.UpdateNodeDescription(nodes.Get(0), "Server");

  for (uint32_t i = 1; i < 9; ++i) {
    anim.SetConstantPosition(nodes.Get(i), i * 10.0, 0.0);
    anim.UpdateNodeDescription(nodes.Get(i), "Client " + std::to_string(i));
    anim.UpdateNodeColor(nodes.Get(i), 255 / (i + 1), 0, 255 * i);
    anim.UpdateNodeSize(nodes.Get(i), 5.0, 5.0);
  }

  anim.SetConstantPosition(nodes.Get(9), 40.0, 0.0);
  anim.UpdateNodeDescription(nodes.Get(9), "Access Point");
  anim.UpdateNodeColor(nodes.Get(9), 255, 0, 0);
  anim.UpdateNodeSize(nodes.Get(9), 10.0, 10.0);
  
  Simulator::Schedule(Seconds(2.0), [&](void) {
  anim.UpdateNodeDescription(nodes.Get(0), "Server - Request Received");
  });

  for (uint32_t i = 1; i < 9; ++i) {
    Simulator::Schedule(Seconds(2.0 + i), [&](void) {
      anim.UpdateNodeDescription(nodes.Get(i), "Client " + std::to_string(i) + " - Request Sent");
    });
  }

  Simulator::Schedule(Seconds(3.0), [&](void) {
    anim.UpdateNodeDescription(nodes.Get(0), "Server - Response Sent");
  });

  for (uint32_t i = 1; i < 9; ++i) {
    Simulator::Schedule(Seconds(3.0 + i), [&](void) {
      anim.UpdateNodeDescription(nodes.Get(i), "Client " + std::to_string(i) + " - Response Received");
    });
  }

  anim.SetStopTime(Seconds(10.0));
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  
  Simulator::Run();

  // Configurando a animação com o NetAnim
  AnimationInterface anim("anim.xml");
  anim.SetConstantPosition(noSensorUmidade, 0.0, 0.0);
  anim.SetConstantPosition(noSensorTemperatura, 5.0, 0.0);
  anim.SetConstantPosition(noIntermediario, 2.0, 10.0);
  anim.SetConstantPosition(noServidor, 2.0, 10.0);
  anim.SetConstantPosition(noSensorNutrientes, 2.0, 10.0);

  anim.EnablePacketMetadata(true); // Ativar metadados do pacote na animação


  Simulator::Destroy();
  
  return 0;
}