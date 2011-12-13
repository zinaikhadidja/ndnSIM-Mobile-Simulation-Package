/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Hajime Tazaki
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Hajime Tazaki (tazaki@sfc.wide.ad.jp)
 *         Ilya Moiseenko <iliamo@cs.ucla.edu>
 */

#include <fstream>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <regex.h>

#include "ns3/log.h"
#include "rocketfuel-weights-reader.h"

namespace ns3 {
    
    NS_LOG_COMPONENT_DEFINE ("RocketfuelWeightsReader");
    
    NS_OBJECT_ENSURE_REGISTERED (RocketfuelWeightsReader);
    
    TypeId RocketfuelWeightsReader::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::RocketfuelWeightsReader")
        .SetParent<Object> ()
        ;
        return tid;
    }
    
    RocketfuelWeightsReader::RocketfuelWeightsReader ()
    {
        NS_LOG_FUNCTION (this);
    }
    
    RocketfuelWeightsReader::~RocketfuelWeightsReader ()
    {
        NS_LOG_FUNCTION (this);
    }
    
    /* uid @loc [+] [bb] (num_neigh) [&ext] -> <nuid-1> <nuid-2> ... {-euid} ... =name[!] rn */
    
#define REGMATCH_MAX 16
    
#define START "^"
#define END "$"
#define SPACE "[ \t]+"
#define MAYSPACE "[ \t]*"
    
#define ROCKETFUEL_MAPS_LINE \
START "(-*[0-9]+)" SPACE "(@[?A-Za-z0-9,+]+)" SPACE \
"(\\+)*" MAYSPACE "(bb)*" MAYSPACE \
"\\(([0-9]+)\\)" SPACE "(&[0-9]+)*" MAYSPACE \
"->" MAYSPACE "(<[0-9 \t<>]+>)*" MAYSPACE \
"(\\{-[0-9\\{\\} \t-]+\\})*" SPACE \
"=([A-Za-z0-9.!-]+)" SPACE "r([0-9])" \
MAYSPACE END
    
#define ROCKETFUEL_WEIGHTS_LINE \
START "([^ \t]+)" SPACE "([^ \t]+)" SPACE "([0-9.]+)" MAYSPACE END
    
    int linksNumber = 0;
    int nodesNumber = 0;
    std::map<std::string, Ptr<Node> > nodeMap;
    
    NodeContainer
    RocketfuelWeightsReader::GenerateFromWeightsFile (int argc, char *argv[], char *argl[])
    {
        /* uid @loc [+] [bb] (num_neigh) [&ext] -> <nuid-1> <nuid-2> ... {-euid} ... =name[!] rn */
        std::string sname;
        std::string tname;
        char *endptr;
        NodeContainer nodes;
        
        sname = argv[0];
        tname = argv[1];
        double v = strtod (argv[2], &endptr); // weight
        double l = strtod (argl[2], &endptr); //latency
        // cast v to void , to suppress 'v' set but not used compiler warning
        //(void) v;
        
        
        
        if (*endptr != '\0')
        {
            NS_LOG_WARN ("invalid weight: " << argv[2]);
            return nodes;
        }
        
        // Create node and link
        if (!sname.empty () && !tname.empty ())
        {
            if (nodeMap[sname] == 0)
            {
                Ptr<Node> tmpNode = CreateObject<Node> ();
                nodeMap[sname] = tmpNode;
                nodes.Add (tmpNode);
                nodesNumber++;
            }
            
            if (nodeMap[tname] == 0)
            {
                Ptr<Node> tmpNode = CreateObject<Node> ();
                nodeMap[tname] = tmpNode;
                nodes.Add (tmpNode);
                nodesNumber++;
            }
            NS_LOG_INFO (linksNumber << ":" << nodesNumber << " From: " << sname << " to: " << tname);
            TopologyReader::ConstLinksIterator iter;
            bool found = false;
            for (iter = LinksBegin (); iter != LinksEnd (); iter++)
            {
                if ((iter->GetFromNode () == nodeMap[tname])
                    && (iter->GetToNode () == nodeMap[sname]))
                {
                    found = true;
                    break;
                }
            }
            
            if (!found)
            {
                Link link (nodeMap[sname], sname, nodeMap[tname], tname);
                
                std::ostringstream s;
                s << std::setprecision(2) << v;
                std::string ss = s.str();
                link.SetAttribute ("OSPF", ss);
                
                std::ostringstream s2;
                s2 << std::setprecision(2) << l;
                std::string ss2 = s2.str();
                link.SetAttribute("Latency", ss2);
                NS_LOG_INFO("Written Latency = " << ss2);
                
                
                AddLink (link);
                linksNumber++;
            }
        }
        return nodes;
    }
    
    enum RocketfuelWeightsReader::RF_FileType
    RocketfuelWeightsReader::GetFileType (const char *line)
    {
        int ret;
        regmatch_t regmatch[REGMATCH_MAX];
        regex_t regex;
        char errbuf[512];
        
        // Check whether MAPS file or not
        ret = regcomp (&regex, ROCKETFUEL_MAPS_LINE, REG_EXTENDED | REG_NEWLINE);
        if (ret != 0)
        {
            regerror (ret, &regex, errbuf, sizeof (errbuf));
            return RF_UNKNOWN;
        }
        ret = regexec (&regex, line, REGMATCH_MAX, regmatch, 0);
        if (ret != REG_NOMATCH)
        {
            regfree (&regex);
            return RF_MAPS;
        }
        regfree (&regex);
        
        // Check whether Weights file or not
        ret = regcomp (&regex, ROCKETFUEL_WEIGHTS_LINE, REG_EXTENDED | REG_NEWLINE);
        if (ret != 0)
        {
            regerror (ret, &regex, errbuf, sizeof (errbuf));
            return RF_UNKNOWN;
        }
        ret = regexec (&regex, line, REGMATCH_MAX, regmatch, 0);
        if (ret != REG_NOMATCH)
        {
            regfree (&regex);
            return RF_WEIGHTS;
        }
        regfree (&regex);
        
        return RF_UNKNOWN;
    }
    
    NodeContainer
    RocketfuelWeightsReader::Read (void)
    {
        NodeContainer nodes;
        NS_LOG_INFO("Not implemented");
        return nodes;
    }
    
    NodeContainer
    RocketfuelWeightsReader::Read (std::string latenciesFile)
    {
        std::ifstream topgen;
        topgen.open (GetFileName ().c_str ());
        NodeContainer nodes;
        
        std::ifstream latencies;
        latencies.open(latenciesFile.c_str());
        
        std::istringstream lineBuffer;
        std::istringstream lineBuffer2;
        std::string line;
        std::string line2;
        
        int lineNumber = 0;
        enum RF_FileType ftype = RF_UNKNOWN;
        char errbuf[512];
        
        if (!topgen.is_open ())
        {
            NS_LOG_WARN ("Couldn't open the file " << GetFileName ());
            return nodes;
        }
        
        if (!latencies.is_open ())
        {
            NS_LOG_WARN ("Couldn't open Latencies file " << latenciesFile);
            return nodes;
        }
        
        while (!topgen.eof ())
        {
            int ret;
            int argc;
            char *argv[REGMATCH_MAX];
            char *argl[REGMATCH_MAX];
            char *buf;
            char *buf2;
            
            lineNumber++;
            line.clear ();
            line2.clear ();
            lineBuffer.clear ();
            lineBuffer2.clear ();
            
            getline (topgen, line);
            getline (latencies, line2);
            buf = (char *)line.c_str ();
            buf2 = (char *)line2.c_str ();
            
            if (lineNumber == 1)
            {
                ftype = GetFileType (buf);
                if (ftype == RF_UNKNOWN)
                {
                    NS_LOG_INFO ("Unknown File Format (" << GetFileName () << ")");
                    break;
                }
            }
            
            regmatch_t regmatch[REGMATCH_MAX];
            regex_t regex;
            
            if (ftype == RF_MAPS)
            {
                ret = regcomp (&regex, ROCKETFUEL_MAPS_LINE, REG_EXTENDED | REG_NEWLINE);
                if (ret != 0)
                {
                    regerror (ret, &regex, errbuf, sizeof (errbuf));
                    regfree (&regex);
                    break;
                }
                
                ret = regexec (&regex, buf, REGMATCH_MAX, regmatch, 0);
                if (ret == REG_NOMATCH)
                {
                    NS_LOG_WARN ("match failed (maps file): %s" << buf);
                    regfree (&regex);
                    break;
                }
            }
            else if (ftype == RF_WEIGHTS)
            {
                ret = regcomp (&regex, ROCKETFUEL_WEIGHTS_LINE, REG_EXTENDED | REG_NEWLINE);
                if (ret != 0)
                {
                    regerror (ret, &regex, errbuf, sizeof (errbuf));
                    regfree (&regex);
                    break;
                }
                
                ret = regexec (&regex, buf, REGMATCH_MAX, regmatch, 0);
                regexec (&regex, buf2, REGMATCH_MAX, regmatch, 0);
                if (ret == REG_NOMATCH)
                {
                    NS_LOG_WARN ("match failed (weights file): %s" << buf);
                    regfree (&regex);
                    break;
                }
            }
            
            line = buf;
            line2 = buf2;
            argc = 0;
            
            /* regmatch[0] is the entire strings that matched */
            for (int i = 1; i < REGMATCH_MAX; i++)
            {
                if (regmatch[i].rm_so == -1)
                {
                    argv[i - 1] = NULL;
                    argl[i - 1] = NULL;
                }
                else
                {
                    line[regmatch[i].rm_eo] = '\0';
                    line2[regmatch[i].rm_eo] = '\0';
                    argv[i - 1] = &line[regmatch[i].rm_so];
                    argl[i - 1] = &line2[regmatch[i].rm_so];
                    argc = i;
                }
            }
            
            if (ftype == RF_MAPS)
            {
                NS_LOG_INFO("MAPS FILE is not supported");
                //nodes.Add (GenerateFromMapsFile (argc, argv));
            }
            else if (ftype == RF_WEIGHTS)
            {
                nodes.Add (GenerateFromWeightsFile (argc, argv, argl));
            }
            else
            {
                NS_LOG_WARN ("Unsupported file format (only Maps/Weights are supported)");
            }
            
            regfree (&regex);
        }
        
        
        topgen.close ();
        latencies.close ();
        NS_LOG_INFO ("Rocketfuel topology created with " << nodesNumber << " nodes and " << linksNumber << " links");
        return nodes;
    }
    
    
void
RocketfuelWeightsReader::ApplySettings(NetDeviceContainer* ndc, NodeContainer* nc)
{
  InternetStackHelper stack;
  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");
        
  Ipv4GlobalRoutingHelper ipv4RoutingHelper ("ns3::Ipv4GlobalRoutingOrderedNexthops");
  stack.SetRoutingHelper (ipv4RoutingHelper);
        
  //This loop passes all links and checks if ipv4 is installed on the node
  // if not, it installs. 
  // We can't use stack.Install(nc) because in nc there are duplicates and assertion fails
        TopologyReader::ConstLinksIterator iter;
        int j = 0;
        for ( iter = this->LinksBegin (); iter != this->LinksEnd (); iter++, j++ )
        {
            NodeContainer twoNodes = nc[j];
            
            Ptr<Node> nd = twoNodes.Get(0);
            if(nd==NULL)
                NS_LOG_INFO("nd = null");
            
            Ptr<Node> nd2 = twoNodes.Get(1);
            if(nd2==NULL)
                NS_LOG_INFO("nd2 = null");
            
            Ptr<Ipv4> ipv4 = nd->GetObject<Ipv4>();
            if(ipv4 == 0)
            {
                NS_LOG_INFO("ipv4 = null");
                stack.Install(nd);
            }
            
            Ptr<Ipv4> ipv42 = nd2->GetObject<Ipv4>();
            if(ipv42 == 0)
            {
                NS_LOG_INFO("ipv42 = null");
                stack.Install(nd2);
            }
            
            //NS_LOG_INFO("#netdevices = " << nd->GetNDevices());
            //NS_LOG_INFO("#netdevices = " << nd2->GetNDevices());
        }
        
        NS_LOG_INFO("ITER2");
        
        PointToPointHelper p2p;
        TopologyReader::ConstLinksIterator iter2;
        int i = 0;
        for ( iter2 = this->LinksBegin (); iter2 != this->LinksEnd (); iter2++, i++ )
        {
            p2p.SetDeviceAttribute("DataRate", StringValue("9920000Kbps"));
            //p2p.SetChannelAttribute("Delay", StringValue("10ms"));
            NS_LOG_INFO("Latency = " + iter2->GetAttribute("Latency")+"ms");
            p2p.SetChannelAttribute("Delay", StringValue(iter2->GetAttribute("Latency")+"ms"));
            

            p2p.SetQueue("ns3::DropTailQueue","MaxPackets",StringValue("100"));
            ndc[i] = p2p.Install(nc[i]);
            
            
            NodeContainer twoNodes = nc[i];
            
            Ptr<Node> nd = twoNodes.Get(0);
            if(nd==NULL)
                NS_LOG_INFO("nd = null");
            
            
            
            Ptr<Node> nd2 = twoNodes.Get(1);
            if(nd2==NULL)
                NS_LOG_INFO("nd2 = null");
            
            //NS_LOG_INFO("1");
            NS_LOG_INFO("#netdevices = " << nd->GetNDevices());
            NS_LOG_INFO("#netdevices = " << nd2->GetNDevices());
            
            Ptr<NetDevice> device = nd->GetDevice(nd->GetNDevices()-1)->GetObject<PointToPointNetDevice> ();
            
            if(device==NULL)
                NS_LOG_INFO("device = 0");
            
            std::string ospf = iter2->GetAttribute("OSPF");
            double metric_d = atof(ospf.c_str());
            uint16_t metric = static_cast<int>(metric_d * 10); 
            NS_LOG_INFO("OSPF metric = " << metric);
            
            {
                NetDeviceContainer* temp = new NetDeviceContainer[1];
                temp->Add(device);
                address.Assign (*temp);
            }
            
            Ptr<Ipv4> ipv4 = nd->GetObject<Ipv4>();
            if(ipv4 == 0)
            {
                NS_LOG_INFO("ipv4 = null");
                //stack.Install(nd);
                /*NetDeviceContainer* temp = new NetDeviceContainer[1];
                 temp->Add(device);
                 address.Assign (*temp);
                 ipv4 = nd->GetObject<Ipv4>();*/
            }
            
            NS_LOG_INFO("Before GetID");
            int32_t interfaceId = ipv4->GetInterfaceForDevice(device);
            NS_LOG_INFO("InterfaceID = " << interfaceId);
            ipv4->SetMetric(interfaceId,metric);
            
            
            
            
            
            /*Ptr<Ipv4> ipv4 = nd->GetObject<Ipv4>();
             
             if(ipv4 == 0)
             NS_LOG_INFO("ipv4 = null");
             int32_t interfaceId = ipv4->GetInterfaceForDevice(device);
             ipv4->SetMetric(interfaceId,metric);*/
            
            //Ptr<Ipv4Interface> interface = nd->GetDevice(nd->GetNDevices()-1)->GetObject<Ipv4Interface> ();
            //ipv4->SetMetric(metric);
            
            //NS_LOG_INFO("2");
            
            Ptr<NetDevice> device2 = nd2->GetDevice(nd2->GetNDevices()-1)->GetObject<PointToPointNetDevice> ();
            
            if(device2==NULL)
                NS_LOG_INFO("device2 = 0");
            
            {
                NetDeviceContainer* temp = new NetDeviceContainer[1];
                temp->Add(device2);
                address.Assign (*temp);
            }
            
            Ptr<Ipv4> ipv42 = nd2->GetObject<Ipv4>();
            if(ipv42 == 0)
            {
                NS_LOG_INFO("ipv42 = null");
                /*stack.Install(nd2);
                 NetDeviceContainer* temp = new NetDeviceContainer[1];
                 temp->Add(device2);
                 address.Assign (*temp);
                 ipv42 = nd2->GetObject<Ipv4>();*/
            }
            
            NS_LOG_INFO("Before GetID");
            interfaceId = ipv42->GetInterfaceForDevice(device2);
            NS_LOG_INFO("InterfaceID = " << interfaceId);
            ipv42->SetMetric(interfaceId,metric);
            
            
            
            /*PointerValue tmp1;
            device->GetAttribute ("TxQueue", tmp1);
            //NS_LOG_INFO("2.5");
            Ptr<Object> txQueue1 = tmp1.GetObject ();
            
            PointerValue tmp2;
            device2->GetAttribute ("TxQueue", tmp2);
            Ptr<Object> txQueue2 = tmp2.GetObject ();
            //NS_LOG_INFO("3");
            Ptr<DropTailQueue> dtq1 = txQueue1->GetObject <DropTailQueue> ();
            NS_ASSERT (dtq1 != 0);
            
            Ptr<DropTailQueue> dtq2 = txQueue2->GetObject <DropTailQueue> ();
            NS_ASSERT (dtq2 != 0);
            
            std::string queuesize1 = iter2->GetAttribute("QueueSizeNode1");
            std::string queuesize2 = iter2->GetAttribute("QueueSizeNode2");
            //NS_LOG_INFO("4");
            txQueue1->SetAttribute("MaxPackets", UintegerValue (atoi(queuesize1.c_str())));
            txQueue2->SetAttribute("MaxPackets", UintegerValue (atoi(queuesize2.c_str())));
            
            UintegerValue limit;
            txQueue1->GetAttribute ("MaxPackets", limit);
            NS_LOG_INFO ("NetDevice #"<< device->GetIfIndex() << "has queue limit " << limit.Get () << " packets");
            
            txQueue2->GetAttribute ("MaxPackets", limit);
            NS_LOG_INFO ("NetDevice #"<< device2->GetIfIndex() << "has queue limit " << limit.Get () << " packets");*/
        }
    }

    
} /* namespace ns3 */

