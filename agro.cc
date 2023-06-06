/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include <iostream>
#include <fstream> 
#include <iomanip>
#include <chrono>
#include <thread>
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-metadata.h"
#include "ns3/netanim-module.h"

#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"

using namespace ns3;
using namespace std::chrono_literals;

NS_LOG_COMPONENT_DEFINE ("ProjetoAgro");

/**
 * Receive pacote no nó intermediário
 * \param socket Rx socket
 */
void ReceivePacket (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);
  
  std::cout << "Pacote recebido no nó intermédiario, enviando para servidor...\n" << std::endl;
  socket->Send (pkt);
}

std::string packetSender(std::string content) {
  std::string sensor;
  if (content.find("Umidade") != -1) sensor = "Umidade";
  else if (content.find("Temperatura") != -1) sensor = "Temperatura";
  else if (content.find("Nutrientes") != -1) sensor = "Nutrientes";

  return sensor;
}

/**
 * Recebe pacote no nó servidor
 * \param socket Rx socket
 */
void ReceivePacketServer (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);
  std::string sensor = packetSender(s);
  std::cout << "Pacote de " << sensor << " recebido no nó servidor" << std::endl;
  if (s.find('0') != -1) std::cout << sensor << " fora do esperado\nEnviando sinal para no atuador\n\n" << std::endl;
  else std::cout << sensor << " OK\n\n" << std::endl;
}

/**
 * Geerate traffic
 * \param socket Tx socket
 * \param pktSize packet size
 * \param pktCount number of packets
 * \param pktInterval interval between packet generation
 */
static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval, Ptr<Packet> pkt) {
  if (pktCount > 0) {
    uint8_t *buffer = new uint8_t[pkt->GetSize ()];
    pkt->CopyData(buffer, pkt->GetSize ());
    std::string s = std::string((char*)buffer);
    std::string sensor = packetSender(s);
    std::cout << "Enviando leitura média de " << sensor << std::endl;
    socket->Send (pkt);
    Simulator::Schedule (pktInterval, &GenerateTraffic, socket, pktSize,pktCount - 1, pktInterval, pkt);
  } else {
    socket->Close();
  }
}

int main (int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  std::cout << "Lendo arquivo de teste" << std::endl;

  std::ifstream file;
  file.open("teste1.txt", std::ios::in);
  if (!file.is_open()) {
        std::cout << "Erro ao abrir o arquivo." << std::endl;
        return 1;
  }

  std::cout << "Leu arquivo de teste" << std::endl;

  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1;
  double interval = 1000;

  ns3::PacketMetadata::Enable();
  Time interPacketInterval = Seconds (interval);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);


  std::cout << "Definindo nós sensores, servidor e intermediário" << std::endl;

  // Definição dos nós
  Ptr<Node> noSensorUmidade = CreateObject<Node>();
  Ptr<Node> noSensorTemperatura = CreateObject<Node>();
  Ptr<Node> noSensorNutrientes = CreateObject<Node>();
  Ptr<Node> noIntermediario = CreateObject<Node>();
  Ptr<Node> noServidor = CreateObject<Node>();

  std::cout << "Definindo subrede 1 - A primeira estufa" << std::endl;

  // Container da estufa 1
  NodeContainer fluxo(noSensorUmidade, noSensorTemperatura, noIntermediario, noServidor, noSensorNutrientes);

  std::cout << "Configurando camada física e o canal para a rede criada" << std::endl;

  // configurando camada fisica e o canal da rede
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  Ptr<YansWifiChannel> channel = wifiChannel.Create ();
  wifiPhy.SetChannel (channel);
  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();

  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue (phyMode),
                                      "ControlMode",StringValue (phyMode));

  std::cout << "Conectando estufa 1 a rede" << std::endl;
  // conectando estufa 1 a rede
  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, fluxo);




  std::cout << "Posicionando os nós"  << std::endl;
  // configurando a posicao de cada no
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 5.0, 0.0));
  positionAlloc->Add (Vector (5.0, 10.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (fluxo);

  InternetStackHelper internet;
  internet.Install (fluxo);

  std::cout << "Definindo endereços" << std::endl;
  // definindo endereços dos dispositivos da rede
  Ipv4AddressHelper ipv4Estufas;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4Estufas.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4Estufas.Assign (devices);
  
  std::cout << "Configurando recebimento do pacote no Servidor" << std::endl;
  // configurando o recebimento do pacote no nó servidor
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> socketServidor = Socket::CreateSocket (fluxo.Get (3), tid);
  InetSocketAddress localServidor = InetSocketAddress (Ipv4Address::GetAny (), 80);
  socketServidor->Bind(localServidor);
  socketServidor->SetRecvCallback (MakeCallback (&ReceivePacketServer));

  std::cout << "Configurando recebimento do pacote no nó intermediário" << std::endl;
  // configurando o recebimento do pacote no nó intermediário
  Ptr<Socket> socketIntermediario = Socket::CreateSocket (fluxo.Get (2), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  InetSocketAddress remoteServer = InetSocketAddress (Ipv4Address ("10.1.1.0"), 80);
  socketIntermediario->Connect (remoteServer);
  socketIntermediario->Bind(local);
  socketIntermediario->SetRecvCallback (MakeCallback (&ReceivePacket));

  InetSocketAddress remoteIntermediario = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  
  // confugurando sensor de umidade da estufa 1
  Ptr<Socket> sensorUmidade = Socket::CreateSocket (fluxo.Get (0), tid);
  sensorUmidade->SetAllowBroadcast (true);
  sensorUmidade->Connect (remoteIntermediario);

  // configurando sensor de temperatura da estufa 1
  Ptr<Socket> sensorTemperatura = Socket::CreateSocket (fluxo.Get (1), tid);
  sensorTemperatura->SetAllowBroadcast (true);
  sensorTemperatura->Connect (remoteIntermediario);

  // configurando sensor de nutrientes da estufa 1
  Ptr<Socket> sensorNutrientes = Socket::CreateSocket (fluxo.Get (4), tid);
  sensorNutrientes->SetAllowBroadcast (true);
  sensorNutrientes->Connect (remoteIntermediario);

  int sensor = 0;
  int timer = 0;
  char myText;
  while (file.get(myText)) {

    std::ostringstream msg;
    std::string content;

    if (sensor == 0) content = "Umidade ";
    else if (sensor == 1) content = "Temperatura ";
    else if (sensor == 2) content = "Nutrientes ";

    msg << content + myText << '\0';
    Ptr<Packet> packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
    if (sensor == 0) {
      Simulator::ScheduleWithContext (sensorUmidade->GetNode ()->GetId (),
                                Seconds (timer * 1.5), &GenerateTraffic,
                                sensorUmidade, packetSize, numPackets, interPacketInterval, packet); 
    } else if (sensor == 1) {
      Simulator::ScheduleWithContext (sensorTemperatura->GetNode ()->GetId (),
                                Seconds (timer * 1.5), &GenerateTraffic,
                                sensorTemperatura, packetSize, numPackets, interPacketInterval, packet); 
    } else {
      Simulator::ScheduleWithContext (sensorNutrientes->GetNode ()->GetId (),
                                Seconds (timer * 1.5), &GenerateTraffic,
                                sensorNutrientes, packetSize, numPackets, interPacketInterval, packet); 
    }

    if (sensor < 2) sensor++;
    else sensor = 0;

    timer++;
  }  

  Simulator::Run ();

  // Configurando a animação com o NetAnim
  AnimationInterface anim("anim.xml");
  anim.SetConstantPosition(noSensorUmidade, 0.0, 0.0);
  anim.SetConstantPosition(noSensorTemperatura, 5.0, 0.0);
  anim.SetConstantPosition(noIntermediario, 10.0, 0.0);
  anim.SetConstantPosition(noServidor, 5.0, 5.0);
  anim.SetConstantPosition(noSensorNutrientes, 5.0, 10.0);

  anim.EnablePacketMetadata(true); // Ativar metadados do pacote na animação

  Simulator::Destroy ();
}
