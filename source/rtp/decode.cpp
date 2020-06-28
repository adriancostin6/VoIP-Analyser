#include "decode.h"

void add_to_buffer(Rtp& rtp_packet, std::list<Rtp>& buff)
{

    if(buff.empty())
    {
        buff.push_back(rtp_packet);
        return;
    }

    auto it = buff.begin();
    for(; it != buff.end();)
    {
        if(rtp_packet.get_seq_no() > (*it).get_seq_no())
            ++it;
        else if(rtp_packet.get_seq_no() < (*it).get_seq_no())
        {
            buff.insert(++it,rtp_packet);
            return;
        }
        else return;
    }

    buff.push_back(rtp_packet);
}


Rtp get_buffer_packet(std::list<Rtp>& buff)
{
    auto it = buff.begin();
    //get pointer to begining of buffer and return it 
    auto res = *it;
    buff.pop_front();
    return res;
}

void decode(
        const std::string& in_filename,
        const std::string& out_filename,
        std::string& src_ip,
        std::string& dst_ip,
        std::string& src_port,
        std::string& dst_port
        )
{

    Tins::SnifferConfiguration conf;
    conf.set_promisc_mode(true);
    conf.set_immediate_mode(true);
    std::string filter = "src host ";
    filter+= src_ip;
    filter += " && ";
    filter += "dst host ";
    filter+= dst_ip; 
    filter += " && ";
    filter += "src port ";
    filter += src_port;
    filter += " && ";
    filter += "dst port ";
    filter += dst_port; 
    conf.set_filter(filter);
    Tins::FileSniffer fsniffer_rtp(in_filename, conf);
    Capture cap_rtp(Capture::CaptureType::IS_RTP, out_filename);
    cap_rtp.run_file_sniffer(fsniffer_rtp);

    std::vector<Rtp> rtp_elems = cap_rtp.get_rtp_packets();


    std::string in_file = in_filename;
    std::string out_file = "../audio/" +  out_filename + ".wav";
    FILE* fp = fopen(out_file.c_str(), "w");
    if(!fp)
    {
        printf("open file fail\n");
    }

    wav_header wav_h;
    memset((void*)&wav_h, 0xff, sizeof(wav_h));
    fwrite((void*)&wav_h, 1, sizeof(wav_h), fp);//the space for wav header


    uint32_t pcm_len = 0;
    uint32_t sample_rate = 0;

    std::list<Rtp> buffer;

    std::shared_ptr<codec> c;
    
    Rtp current_packet;
    auto itr = rtp_elems.begin();
    while(true)
    {
        //this shit gets altered and the data is modified 
        while(buffer.empty())
        {
            for(; itr != rtp_elems.end();)
            {
                add_to_buffer(*itr, buffer);
                ++itr;
                break;
            }
            if(itr == rtp_elems.end())
                break;
        }
        if(itr == rtp_elems.end())
            break;

        current_packet = get_buffer_packet(buffer);

        printf("get rtp packet, payload type: %u, seq: %u, timestamp: %u, data len: %lu\n",
                current_packet.get_payload_type(),
                current_packet.get_seq_no(),
                current_packet.get_timestamp(),
                current_packet.get_data().size()
                );

        c = codec::get_codec_by_payload_type(current_packet.get_payload_type());
        sample_rate = c->get_sample_rate();
        
        if(!c)
        {
            printf("can not get codec for payload type: %u\n", current_packet.get_payload_type());
            continue;
        }

        std::string &&result = c->decode(std::string(""),current_packet.get_data());
        fwrite(result.c_str(), 1, result.size(), fp);
        pcm_len += result.size();

    };

    //set wav header
    memcpy(wav_h.chunk_id, "RIFF", 4);
    memcpy(wav_h.format, "WAVE", 4);
    wav_h.chunk_size = sizeof(wav_h) + pcm_len - 8;

    memcpy(wav_h.fmt_chunk.sub_chunk_id, "fmt ", 4);
    wav_h.fmt_chunk.sub_chunk_size = sizeof(wav_fmt_chunk) - 8;
    wav_h.fmt_chunk.audio_format = 1;
    wav_h.fmt_chunk.num_channels = 1;
    wav_h.fmt_chunk.sample_rate = sample_rate;
    wav_h.fmt_chunk.byte_rate = wav_h.fmt_chunk.sample_rate * 2;
    wav_h.fmt_chunk.block_align = 2;
    wav_h.fmt_chunk.bits_per_sample = 16;
    memcpy(wav_h.data_chunk.sub_chunk_id, "data", 4);
    wav_h.data_chunk.sub_chunk_size = pcm_len;

    fseek(fp, 0, SEEK_SET);
    fwrite((void*)&wav_h, 1, sizeof(wav_h), fp);//write wav header
    printf("wav header size: %d\n", sizeof(wav_h));

    fclose(fp);

}