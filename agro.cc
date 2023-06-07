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
#include <sys/socket.h>
#include <netinet/in.h>

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
  if (s.find('0') != -1){
    std::cout << sensor << " fora do esperado.\nEnviando sinal para atuador de " << sensor << std::endl;
    if (sensor == "Temperatura") {
      // Enviar para atuador de temperatura
      Ptr<Packet> responsePkt = Create<Packet>(reinterpret_cast<const uint8_t*>("Sinal para atuador de temperatura"), 30);
      socket->Send(responsePkt);
    } else if (sensor == "Umidade") {
      // Enviar para atuador de umidade
      Ptr<Packet> responsePkt = Create<Packet>(reinterpret_cast<const uint8_t*>("Sinal para atuador de umidade"), 28);
      socket->Send(responsePkt);
    } else {
      // Enviar para atuador de nutrientes
      Ptr<Packet> responsePkt = Create<Packet>(reinterpret_cast<const uint8_t*>("Sinal para atuador de nutrientes"), 31);
      socket->Send(responsePkt);
    }
  } 
  else{
     std::cout << sensor << " OK\n" << std::endl;
  }
}

/**
 * Geerate traffic
 * \param socket Tx socket
 * \param pktSize packet size
 * \param pktCount number of packets
 * \param pktInterval interval between packet generation
 */
static void GenerateTraffic(Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval, Ptr<Packet> pkt) {
  if (pktCount > 0) {
    uint8_t *buffer = new uint8_t[pkt->GetSize()];
    pkt->CopyData(buffer, pkt->GetSize());
    std::string s = std::string((char*)buffer);
    std::string sensor = packetSender(s);
    std::cout << "Enviando leitura média de " << sensor << std::endl;
    socket->Send(pkt);
    Simulator::Schedule(pktInterval, &GenerateTraffic, socket, pktSize, pktCount - 1, pktInterval, pkt);
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
  Ptr<Node> noSensorUmidade1 = CreateObject<Node>();
  Ptr<Node> noSensorTemperatura1 = CreateObject<Node>();
  Ptr<Node> noSensorNutrientes1 = CreateObject<Node>();
  Ptr<Node> noIntermediario1 = CreateObject<Node>();
  Ptr<Node> noAtuadorUmidade1 = CreateObject<Node>();
  Ptr<Node> noAtuadorTemperatura1 = CreateObject<Node>();
  Ptr<Node> noAtuadorNutrientes1 = CreateObject<Node>();

  Ptr<Node> noSensorUmidade2 = CreateObject<Node>();
  Ptr<Node> noSensorTemperatura2 = CreateObject<Node>();
  Ptr<Node> noSensorNutrientes2 = CreateObject<Node>();
  Ptr<Node> noIntermediario2 = CreateObject<Node>();
  Ptr<Node> noAtuadorUmidade2 = CreateObject<Node>();
  Ptr<Node> noAtuadorTemperatura2 = CreateObject<Node>();
  Ptr<Node> noAtuadorNutrientes2 = CreateObject<Node>();

  Ptr<Node> noServidorCentral = CreateObject<Node>();


  std::cout << "Definindo subrede 1 - A primeira estufa" << std::endl;

  // Container da estufa 1
  NodeContainer estufa1(noSensorUmidade1, noSensorTemperatura1, noIntermediario1, noSensorNutrientes1, noAtuadorUmidade1, noAtuadorTemperatura1, noAtuadorNutrientes1);

  std::cout << "Definindo subrede 2 - Estufa 2" << std::endl;

  // Container da estufa 2
  NodeContainer estufa2(noSensorUmidade2, noSensorTemperatura2, noIntermediario2, noSensorNutrientes2, noAtuadorUmidade2, noAtuadorTemperatura2, noAtuadorNutrientes2);

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

  std::cout << "Conectando estufa 1 à rede" << std::endl;
  // Conectando estufa 1 à rede
  NetDeviceContainer devicesEstufa1 = wifi80211p.Install(wifiPhy, wifi80211pMac, estufa1);

  std::cout << "Conectando estufa 2 à rede" << std::endl;
  // Conectando estufa 2 à rede
  NetDeviceContainer devicesEstufa2 = wifi80211p.Install(wifiPhy, wifi80211pMac, estufa2);

  std::cout << "Conectando servidor central à rede" << std::endl;
  // Conectando servidor central à rede
  NetDeviceContainer devicesServidorCentral = wifi80211p.Install(wifiPhy, wifi80211pMac, noServidorCentral);


  std::cout << "Posicionando os nós"  << std::endl;
  // configurando a posicao de cada no
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));
  positionAlloc->Add (Vector (0.0, 2.5, 0.0));
  positionAlloc->Add (Vector (5.0, 2.5, 0.0));
  positionAlloc->Add (Vector (10.0, 2.5, 0.0));
  positionAlloc->Add (Vector (5.0, 5.0, 0.0));
  positionAlloc->Add (Vector (5.0, 10.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install(estufa1);
  mobility.Install(estufa2);
  mobility.Install(noServidorCentral);

  InternetStackHelper internet;
  internet.Install(estufa1);
  internet.Install(estufa2);
  internet.Install(noServidorCentral);

  std::cout << "Definindo endereços" << std::endl;
  // definindo endereços dos dispositivos da rede
  Ipv4AddressHelper ipv4Estufas;
  NS_LOG_INFO("Assign IP Addresses for Estufa1.");
  ipv4Estufas.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iEstufa1 = ipv4Estufas.Assign(devicesEstufa1);

  NS_LOG_INFO("Assign IP Addresses for Estufa2.");
  ipv4Estufas.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iEstufa2 = ipv4Estufas.Assign(devicesEstufa2);

  NS_LOG_INFO("Assign IP Addresses for Servidor Central.");
  ipv4Estufas.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iServidorCentral = ipv4Estufas.Assign(devicesServidorCentral);
  
  std::cout << "Configurando recebimento do pacote no Servidor" << std::endl;
  // configurando o recebimento do pacote no nó servidor
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  Ptr<Socket> socketServidor = Socket::CreateSocket (noServidorCentral, tid);
  InetSocketAddress localServidor = InetSocketAddress (Ipv4Address::GetAny(), 80);
  socketServidor->Bind(localServidor);
  socketServidor->SetRecvCallback (MakeCallback (&ReceivePacketServer));

  std::cout << "Configurando recebimento do pacote no nó intermediário" << std::endl;
  // Nó intermediário 1
  Ptr<Socket> socketIntermediario1 = Socket::CreateSocket(estufa1.Get(2), tid);
  InetSocketAddress localIntermediario1 = InetSocketAddress(Ipv4Address::GetAny(), 80);
  InetSocketAddress remoteServidorCentral1 = InetSocketAddress(Ipv4Address("10.1.3.0"), 80);
  socketIntermediario1->Connect(remoteServidorCentral1);
  socketIntermediario1->Bind(localIntermediario1);
  socketIntermediario1->SetRecvCallback(MakeCallback(&ReceivePacket));

  // Nó intermediário 2
  Ptr<Socket> socketIntermediario2 = Socket::CreateSocket(estufa2.Get(2), tid);
  InetSocketAddress localIntermediario2 = InetSocketAddress(Ipv4Address::GetAny(), 80);
  InetSocketAddress remoteServidorCentral2 = InetSocketAddress(iServidorCentral.GetAddress(0), 80);
  socketIntermediario2->Connect(remoteServidorCentral2);
  socketIntermediario2->Bind(localIntermediario2);
  std::cout << "Configurando envio de pacote dos sensores para os nós intermediários" << std::endl;
  
  // Configurando envio de pacote dos sensores para os nós intermediários
  InetSocketAddress remoteIntermediario = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);

  // Sensores da umidade da estufa 1
  Ptr<Socket> sensorUmidade1 = Socket::CreateSocket(estufa1.Get(0), tid);
  sensorUmidade1->SetAllowBroadcast(true);
  sensorUmidade1->Connect(remoteIntermediario);

  Ptr<Socket> sensorTemperatura1 = Socket::CreateSocket(estufa1.Get(1), tid);
  sensorTemperatura1->SetAllowBroadcast(true);
  sensorTemperatura1->Connect(remoteIntermediario);

  Ptr<Socket> sensorNutrientes1 = Socket::CreateSocket(estufa1.Get(3), tid);
  sensorNutrientes1->SetAllowBroadcast(true);
  sensorNutrientes1->Connect(remoteIntermediario);


  // Atuadores da umidade da estufa 1
  Ptr<Socket> atuadorUmidade1 = Socket::CreateSocket(estufa1.Get(4), tid);
  atuadorUmidade1->SetAllowBroadcast(true);
  atuadorUmidade1->Connect(remoteIntermediario);

  Ptr<Socket> atuadorTemperatura1 = Socket::CreateSocket(estufa1.Get(5), tid);
  atuadorTemperatura1->SetAllowBroadcast(true);
  atuadorTemperatura1->Connect(remoteIntermediario);

  Ptr<Socket> atuadorNutrientes1 = Socket::CreateSocket(estufa1.Get(6), tid);
  atuadorNutrientes1->SetAllowBroadcast(true);
  atuadorNutrientes1->Connect(remoteIntermediario);


  // Sensores de umidade da estufa 2
  Ptr<Socket> sensorUmidade2 = Socket::CreateSocket(estufa2.Get(0), tid);
  sensorUmidade2->SetAllowBroadcast(true);
  sensorUmidade2->Connect(remoteIntermediario);

  Ptr<Socket> sensorTemperatura2 = Socket::CreateSocket(estufa2.Get(1), tid);
  sensorTemperatura2->SetAllowBroadcast(true);
  sensorTemperatura2->Connect(remoteIntermediario);

  Ptr<Socket> sensorNutrientes2 = Socket::CreateSocket(estufa2.Get(3), tid);
  sensorNutrientes2->SetAllowBroadcast(true);
  sensorNutrientes2->Connect(remoteIntermediario);


  // Atuadores da umidade da estufa 2
  Ptr<Socket> atuadorUmidade2 = Socket::CreateSocket(estufa1.Get(4), tid);
  atuadorUmidade2->SetAllowBroadcast(true);
  atuadorUmidade2->Connect(remoteIntermediario);

  Ptr<Socket> atuadorTemperatura2 = Socket::CreateSocket(estufa1.Get(5), tid);
  atuadorTemperatura2->SetAllowBroadcast(true);
  atuadorTemperatura2->Connect(remoteIntermediario);

  Ptr<Socket> atuadorNutrientes2 = Socket::CreateSocket(estufa1.Get(6), tid);
  atuadorNutrientes2->SetAllowBroadcast(true);
  atuadorNutrientes2->Connect(remoteIntermediario);


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
      Simulator::ScheduleWithContext (sensorUmidade1->GetNode ()->GetId (),
                                Seconds (timer * 1.5), &GenerateTraffic,
                                sensorUmidade1, packetSize, numPackets, interPacketInterval, packet); 
    } else if (sensor == 1) {
      Simulator::ScheduleWithContext (sensorTemperatura1->GetNode ()->GetId (),
                                Seconds (timer * 1.5), &GenerateTraffic,
                                sensorTemperatura1, packetSize, numPackets, interPacketInterval, packet); 
    } else {
      Simulator::ScheduleWithContext (sensorNutrientes1->GetNode ()->GetId (),
                                Seconds (timer * 1.5), &GenerateTraffic,
                                sensorNutrientes1, packetSize, numPackets, interPacketInterval, packet); 
    }

    if (sensor < 2) sensor++;
    else sensor = 0;

    timer++;
  }  

  Simulator::Run ();

  // Configurando a animação com o NetAnim
  AnimationInterface anim("anim.xml");
  anim.SetConstantPosition(noSensorUmidade1, 0.0, 0.0);
  anim.SetConstantPosition(noSensorTemperatura1, 5.0, 0.0);
  anim.SetConstantPosition(noSensorNutrientes1, 10.0, 0.0);
  anim.SetConstantPosition(noAtuadorUmidade1, 0.0, 2.5);
  anim.SetConstantPosition(noAtuadorTemperatura1, 5.0, 2.5);
  anim.SetConstantPosition(noAtuadorNutrientes1, 10.0, 2.5);
  anim.SetConstantPosition(noIntermediario1, 5.0, 5.0);
  anim.SetConstantPosition(noServidorCentral, 5.0, 10.0);
  anim.SetConstantPosition(noIntermediario2, 5.0, 15.0);
  anim.SetConstantPosition(noSensorUmidade2, 0.0, 17.5);
  anim.SetConstantPosition(noSensorTemperatura2, 5.0, 17.5);
  anim.SetConstantPosition(noSensorNutrientes2, 10.0, 17.5);
  anim.SetConstantPosition(noAtuadorUmidade2, 0.0, 20);
  anim.SetConstantPosition(noAtuadorTemperatura2, 5.0, 20);
  anim.SetConstantPosition(noAtuadorNutrientes2, 10.0, 20);

  anim.EnablePacketMetadata(true); // Ativar metadados do pacote na animação

  Simulator::Destroy ();
}
