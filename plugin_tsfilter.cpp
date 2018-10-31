/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "plugin_tsfilter.h"
#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <vector>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#endif /* _WIN32 */

namespace tsfilter
{
    u_int64_t decode_pts(const unsigned char* p);

    void encode_pts(unsigned char* p,u_int64_t pts,unsigned char prefix);
}

u_int64_t tsfilter::decode_pts(const unsigned char* p)
{
    u_int64_t pts;

    pts = (p[0]>>1)&0x07; pts<<=8;
    pts|= p[1];           pts<<=7;
    pts|= (p[2]>>1)&0x7f; pts<<=8;
    pts|= p[3];           pts<<=7;
    pts|= (p[4]>>1)&0x7f;

    return pts;
}

void tsfilter::encode_pts(unsigned char* p,u_int64_t pts,unsigned char prefix)
{
    p[4]=pts&0x7f; p[4]<<=1; pts>>=7; p[4]|=0x01;
    p[3]=pts&0xff; pts>>=8;
    p[2]=pts&0x7f; p[2]<<=1; pts>>=7; p[2]|=0x01;
    p[1]=pts&0xff; pts>>=8;
    p[0]=pts&0x07; p[0]<<=1; p[0]|=(prefix<<4)|0x01;
}


void tsfilter::sendurl(const std::string&)
{
    int pts_delay=0;

    const char* opts=getenv("OPTS");

    if(opts && *opts)
    {
        pts_delay=atoi(opts);

        if(pts_delay<0)
            pts_delay=0;

        pts_delay*=90;
    }

    std::map<int,int> continuity_counters;

    std::vector<int> pids;

    int program_pid=0;

    unsigned char buf[188];

    while(fread(buf,1,sizeof(buf),stdin)==sizeof(buf))
    {
        if(buf[0]==0x47)
        {
            bool payload_unit_start_indicator   = ( buf[1] & 0x40)?true:false;
            bool adaptation_field_indicator     = ( buf[3] & 0x20)?true:false;
            bool payload_data_indicator         = ( buf[3] & 0x10)?true:false;
            int  pid                            = ((buf[1] & 0x1f) << 8) | buf[2];

            // mask transport error indicator

            buf[1]&=0x7f;

            if(payload_data_indicator)
            {
                // fix continuity counter

                int& continuity_counter=continuity_counters[pid];

                buf[3]=(buf[3] & 0xf0)|(continuity_counter & 0x0f);

                continuity_counter++;

                if(continuity_counter>15)
                    continuity_counter=0;
            }

/*
            unsigned char* payload=buf+4;

            if(adaptation_field_indicator)
                payload+=buf[4]+1;

            if(pid!=0x1fff && payload_data_indicator && payload_unit_start_indicator)
            {
                if(pids.empty())
                {
                    if(!pid || pid==program_pid)
                    {
                        payload+=payload[0]+1;

                        if(pid==0x0000 && payload[0]==0x00)                                             // PAT
                        {
                            int section_length=(((payload[1]&0x03)<<8)|payload[2])-9;

                            payload+=8;

                            if(section_length>0)
                            {
                                for(int i=0;i<section_length/4;i++,payload+=4)
                                {
                                    int program_number=(payload[0]<<8)|payload[1];

                                    int pid=((payload[2]<<8)|payload[3])&0x1fff;

                                    program_pid=pid;

                                    break;
                                }
                            }
                        }else if(pid==program_pid && payload[0]==0x02 && (payload[10]&0xf0)==0xf0)      // PMT
                        {
                            int section_length=(((payload[1]&0x03)<<8)|payload[2])-11;

                            payload+=10;

                            if(section_length>0)
                            {
                                payload+=(((payload[0]&0x0f)<<8)|payload[1])+2;                         // skip program descriptors

                                while(section_length>4 && (payload[1]&0xe0)==0xe0 && (payload[3]&0xf0)==0xf0)
                                {
                                    int type=payload[0];

                                    int pid=((payload[1]&0x1f)<<8)|payload[2];

                                    pids.push_back(pid);

                                    int offset=(((payload[3]&0x03)<<8)|payload[4])+5;

                                    payload+=offset; section_length-=offset;
                                }
                            }
                        }
                    }
                }else
                {
                    bool is_pes=false;

                    for(int i=0;i<pids.size();i++)
                        if(pids[i]==pid)
                            { is_pes=true; break; }

                    if(is_pes && !memcmp(payload,"\x00\x00\x01",3))
                    {
                        if(payload[7]&0x80)                                                             // PTS
                        {
                            u_int64_t pts=decode_pts(payload+9);

                            // fix PTS (delay)
                            pts+=pts_delay;

                            encode_pts(payload+9,pts,(payload[7]&0x40)?0x02:0x03);
                        }

//                        if(payload[7]&0x40)                                                           // DTS
//                        {
//                            u_int64_t dts=decode_pts(payload+14);

//                            encode_pts(payload+14,dts,0x01);
//                        }
                    }
                }
            }
*/

            if(fwrite(buf,1,sizeof(buf),stdout)!=sizeof(buf))
                break;
        }
    }
}

/*
int main(void)
{
    tsfilter::sendurl("");

    return 0;
}
*/
