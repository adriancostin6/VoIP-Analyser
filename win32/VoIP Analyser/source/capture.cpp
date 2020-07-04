#include "capture.h"

#include <fstream>
#include <memory>
#include <functional>

#include "sip.h"

using namespace Tins;

Capture::Capture(CaptureType c, const std::string& filename)
{

    if(c == CaptureType::IS_SIP)
        capture_sip = true;
    else
        capture_sip = false;

    if(c== CaptureType::IS_RTP)
        capture_rtp = true;
    else
        capture_rtp = false;

    //for live capture create a pcap file to store the packets
    std::string file_path = "../temp/" + filename + ".pcap";

    p_writer = std::make_unique<PacketWriter>(file_path,DataLinkType<EthernetII>());
};

bool Capture::callback(const PDU& pdu)
{
    if(loop_stop)
        return false;
    Packet packet = pdu;
    const IP& ip = pdu.rfind_pdu<IP>();
    const UDP& udp = pdu.rfind_pdu<UDP>();
    const RawPDU& raw = udp.rfind_pdu<RawPDU>();

    if(capture_sip)
    {
        std::cout << ip.src_addr() << ":" << udp.sport() << " -> "
            << ip.dst_addr()<< " : " <<udp.dport() << "\n";
        const Sip& sip= raw.to<Sip>();


        //skip empty packets
        if(sip.get_header_order()[0] == "\r")
            return true;

        //write sip packet to a temp file in case we want to look at it in wireshark
        p_writer->write(packet);

        packets_.push_back(sip);

    }
    else
    {
        //std::cout << ip.src_addr() << ":" << udp.sport() << " -> "
        //    << ip.dst_addr()<< " : " <<udp.dport() << "\n";

        //if it is an rtp packet store its port and ip 
        if(capture_rtp)
        {
            const Rtp& rtp_packet = raw.to<Rtp>();
            rtp_packets_.push_back(rtp_packet);
            rtp_ips_and_ports.insert(
                    std::make_pair(
                        std::make_pair(
                            ip.src_addr().to_string(),
                            ip.dst_addr().to_string()
                            ),
                        std::make_pair(
                            std::to_string(udp.sport()),
                            std::to_string(udp.dport())
                            )
                        )
                    );
        }

        p_writer->write(packet);
    }

    return true;
}

void Capture::run_sniffer(Sniffer& sniffer)
{
    sniffer.sniff_loop(std::bind(
                &Capture::callback,
                this,
                std::placeholders::_1
                )
            );
}
void Capture::run_file_sniffer(Tins::FileSniffer& fsniffer) 
{
    fsniffer.sniff_loop(std::bind(
                &Capture::callback,
                this,
                std::placeholders::_1
                )
            );
}


void Capture::print(std::string& path) const
{
    unsigned sz = packets_.size();
    unsigned dif = sz; 
    for(auto const& pack : packets_)
    {
        //each packet will have a separate output file
        //with a name given by "packet_name" + "sz-dif+1"
        //this keeps the output files in ascending order
        if(dif > 0)
        {
            pack.print(path,sz-dif+1);
            dif--;
        }
    }
}

void Capture::print() const
{
    for(auto const& pack: packets_)
        pack.print();
}

std::map<std::pair<std::string,std::string>,
    std::pair<std::string,std::string>> Capture::get_ports()
{
    return rtp_ips_and_ports;
}
std::vector<Rtp> Capture::get_rtp_packets()
{
    return rtp_packets_;
}
std::vector<Sip> Capture::get_sip_packets()
{
    return packets_;
}