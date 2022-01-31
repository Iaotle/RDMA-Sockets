/*
 Copyright 2021 Lin Wang

 This code is part of the Advanced Network Programming (2021) course at 
 Vrije Universiteit Amsterdam.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      https://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/* -*- P4_16 -*- */
#include <core.p4>
#include <v1model.p4>

struct metadata {
    bit<2> count;
}

header ethernet_h {
    bit<48> dstAddr;
    bit<48> srcAddr;
    bit<16> etherType;
}

header ipv4_h {
    bit<4> version;
    bit<4> ihl;
    bit<8> diffserv;
    bit<16> totalLen;
    bit<16> identification;
    bit<3> flags;
    bit<13> fragOffset;
    bit<8> ttl;
    bit<8> protocol;
    bit<16> hdrChecksum;
    bit<32> srcAddr;
    bit<32> dstAddr;
}

header ipv4_options_h {
    varbit<320> options;
} 

header tcp_h {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<32> seqNo;
    bit<32> ackNo;
    bit<4> offset;
    bit<4> reserved;
    bit<8> flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgentPtr;
}

header tcp_options_h {
    varbit<480> options;
}

struct headers {
    ethernet_h eth_hdr;
    ipv4_h ip_hdr;
    ipv4_options_h ip_options;
    tcp_h tcp_hdr;
    tcp_options_h tcp_options;
}

register<bit<2>>(1) myreg;

/*************************************************************************
*********************** P A R S E R  ***********************************
*************************************************************************/

parser MyParser(packet_in packet,
                out headers hdr,
                inout metadata meta,
                inout standard_metadata_t standard_metadata)
{    
    state start {
        transition parse_ethernet;
    }

    state parse_ethernet{
        packet.extract(hdr.eth_hdr);
        transition select(hdr.eth_hdr.etherType) {
            0x800: parse_ipv4;
            default: accept;
        }
    }
    
    state parse_ipv4 {
        packet.extract(hdr.ip_hdr);
        transition select(hdr.ip_hdr.ihl) {
            5: check_protocols;
            default: parse_ipv4_options;
        }
    }

    state parse_ipv4_options {
        packet.extract(hdr.ip_options, (bit<32>)(hdr.ip_hdr.ihl - 5) << 5);
        transition check_protocols;
    }

    state check_protocols {
        transition select(hdr.ip_hdr.protocol) {
            6: parse_tcp;
            default: accept;
        }
    }
        
    state parse_tcp {   
        packet.extract(hdr.tcp_hdr);
        transition select(hdr.tcp_hdr.offset) {
            5: accept;
            default: parse_tcp_options;
        }
    }

    state parse_tcp_options {
        packet.extract(hdr.tcp_options, (bit<32>)(hdr.tcp_hdr.offset - 5) << 5);
        transition accept;
    }

}

/*************************************************************************
************   C H E C K S U M    V E R I F I C A T I O N   *************
*************************************************************************/

control MyVerifyChecksum(inout headers hdr, inout metadata meta) {   
    apply {
        verify_checksum(
	        hdr.ip_hdr.isValid(),
            { hdr.ip_hdr.version,
	          hdr.ip_hdr.ihl,
              hdr.ip_hdr.diffserv,
              hdr.ip_hdr.totalLen,
              hdr.ip_hdr.identification,
              hdr.ip_hdr.flags,
              hdr.ip_hdr.fragOffset,
              hdr.ip_hdr.ttl,
              hdr.ip_hdr.protocol,
              hdr.ip_hdr.srcAddr,
              hdr.ip_hdr.dstAddr},
            hdr.ip_hdr.hdrChecksum,
            HashAlgorithm.csum16
        );
    }
}

/*************************************************************************
**************  I N G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {


    action drop(){
        mark_to_drop(standard_metadata);
    }

    action ipv4_forward(bit<48> dstAddr, bit<9> port){
        hdr.eth_hdr.dstAddr = dstAddr;
        hdr.ip_hdr.ttl = hdr.ip_hdr.ttl - 1;
        standard_metadata.egress_spec = port;
    }

    action ipv4_forward_with_drop(bit<48> dstAddr, bit<9> port){
        myreg.read(meta.count, 0);
        myreg.write(0, meta.count + 1);

        hdr.eth_hdr.dstAddr = dstAddr;
        hdr.ip_hdr.ttl = hdr.ip_hdr.ttl - 1;
        standard_metadata.egress_spec = port;
    }

    table ipv4_lpm{
        key = {
            hdr.ip_hdr.dstAddr: lpm;
        }

        actions = {
            ipv4_forward;
            ipv4_forward_with_drop;
            drop;
        }

        size = 1024;
        default_action = drop();
    }

    table ipv4_ack_lpm{
        key = {
            hdr.ip_hdr.dstAddr: lpm;
        }

        actions = {
            ipv4_forward;
            ipv4_forward_with_drop;
            drop;
        }

        size = 1024;
        default_action = drop();
    }

    apply {
        if(!hdr.tcp_hdr.isValid()){
            drop();
        } else if (hdr.ip_hdr.ttl == 0){
            drop();
        } else if(hdr.tcp_hdr.flags & 16 == 16){
            ipv4_ack_lpm.apply();
        } else {
            ipv4_lpm.apply();
        }

        myreg.read(meta.count, 0);
        if (meta.count == 3){
            myreg.write(0,0);
            drop();
        }
    }
}

/*************************************************************************
****************  E G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control MyEgress(inout headers hdr,
                 inout metadata meta,
                 inout standard_metadata_t standard_metadata) {
    
    apply {

    }
}

/*************************************************************************
*************   C H E C K S U M    C O M P U T A T I O N   **************
*************************************************************************/

control MyComputeChecksum(inout headers  hdr, inout metadata meta) {
     apply {
         update_checksum(
            hdr.ip_hdr.isValid(),
            { hdr.ip_hdr.version,
	          hdr.ip_hdr.ihl,
              hdr.ip_hdr.diffserv,
              hdr.ip_hdr.totalLen,
              hdr.ip_hdr.identification,
              hdr.ip_hdr.flags,
              hdr.ip_hdr.fragOffset,
              hdr.ip_hdr.ttl,
              hdr.ip_hdr.protocol,
              hdr.ip_hdr.srcAddr,
              hdr.ip_hdr.dstAddr},
            hdr.ip_hdr.hdrChecksum,
            HashAlgorithm.csum16);
    }
}

/*************************************************************************
***********************  D E P A R S E R  *******************************
*************************************************************************/

control MyDeparser(packet_out packet, in headers hdr) {
    apply {
        packet.emit(hdr.eth_hdr);
        packet.emit(hdr.ip_hdr);
        packet.emit(hdr.ip_options);
        packet.emit(hdr.tcp_hdr);
        packet.emit(hdr.tcp_options);
    }
}

/*************************************************************************
***********************  S W I T C H  *******************************
*************************************************************************/

V1Switch(
MyParser(),
MyVerifyChecksum(),
MyIngress(),
MyEgress(),
MyComputeChecksum(),
MyDeparser()
) main;
