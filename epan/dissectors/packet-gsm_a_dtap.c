/* packet-gsm_a_dtap.c
 * Routines for GSM A Interface DTAP dissection - A.K.A. GSM layer 3
 * NOTE: it actually includes RR messages, which are (generally) not carried
 * over the A interface on DTAP, but are part of the same Layer 3 protocol set
 *
 * Copyright 2003, Michael Lum <mlum [AT] telostech.com>
 * In association with Telos Technology Inc.
 *
 *
 * Added the GPRS Mobility Managment Protocol and
 * the GPRS Session Managment Protocol
 *   Copyright 2004, Rene Pilz <rene.pilz [AT] ftw.com>
 *   In association with Telecommunications Research Center
 *   Vienna (ftw.)Betriebs-GmbH within the Project Metawin.
 *
 * Added Dissection of Radio Resource Management Information Elements
 * and othere enhancements and fixes.
 * Copyright 2005 - 2006, Anders Broman [AT] ericsson.com
 * Small bugfixes, mainly in Qos and TFT by Nils Ljungberg and Stefan Boman [AT] ericsson.com
 *
 * Title		3GPP			Other
 *
 *   Reference [3]
 *   Mobile radio interface Layer 3 specification;
 *   Core network protocols;
 *   Stage 3
 *   (3GPP TS 24.008 version 4.7.0 Release 4)
 *   (ETSI TS 124 008 V6.8.0 (2005-03))
 *
 *   Reference [4]
 *   Mobile radio interface layer 3 specification;
 *   Radio Resource Control Protocol
 *   (GSM 04.18 version 8.4.1 Release 1999)
 *   (3GPP TS 04.18 version 8.26.0 Release 1999)
 *
 *   Reference [5]
 *   Point-to-Point (PP) Short Message Service (SMS)
 *   support on mobile radio interface
 *   (3GPP TS 24.011 version 4.1.1 Release 4)
 *
 *   Reference [6]
 *   Mobile radio Layer 3 supplementary service specification;
 *   Formats and coding
 *   (3GPP TS 24.080 version 4.3.0 Release 4)
 *
 *   Reference [7]
 *   Mobile radio interface Layer 3 specification;
 *   Core network protocols;
 *   Stage 3
 *   (3GPP TS 24.008 version 5.9.0 Release 5)
 *
 *   Reference [8]
 *   Mobile radio interface Layer 3 specification;
 *   Core network protocols;
 *   Stage 3
 *   (3GPP TS 24.008 version 6.7.0 Release 6)
 *	 (3GPP TS 24.008 version 6.8.0 Release 6)
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <epan/packet.h>
#include <epan/prefs.h>
#include <epan/tap.h>
#include <epan/emem.h>
#include <epan/asn1.h>

#include "packet-bssap.h"
#include "packet-sccp.h"
#include "packet-ber.h"
#include "packet-q931.h"
#include "packet-gsm_a_common.h"
#include "packet-ipv6.h"
#include "packet-e212.h"
#include "packet-ppp.h"

/* PROTOTYPES/FORWARDS */

const value_string gsm_a_dtap_msg_mm_strings[] = {
	{ 0x01,	"IMSI Detach Indication" },
	{ 0x02,	"Location Updating Accept" },
	{ 0x04,	"Location Updating Reject" },
	{ 0x08,	"Location Updating Request" },
	{ 0x11,	"Authentication Reject" },
	{ 0x12,	"Authentication Request" },
	{ 0x14,	"Authentication Response" },
	{ 0x1c,	"Authentication Failure" },
	{ 0x18,	"Identity Request" },
	{ 0x19,	"Identity Response" },
	{ 0x1a,	"TMSI Reallocation Command" },
	{ 0x1b,	"TMSI Reallocation Complete" },
	{ 0x21,	"CM Service Accept" },
	{ 0x22,	"CM Service Reject" },
	{ 0x23,	"CM Service Abort" },
	{ 0x24,	"CM Service Request" },
	{ 0x25,	"CM Service Prompt" },
	{ 0x26,	"Reserved: was allocated in earlier phases of the protocol" },
	{ 0x28,	"CM Re-establishment Request" },
	{ 0x29,	"Abort" },
	{ 0x30,	"MM Null" },
	{ 0x31,	"MM Status" },
	{ 0x32,	"MM Information" },
	{ 0, NULL }
};

const value_string gsm_a_dtap_msg_cc_strings[] = {
	{ 0x01,	"Alerting" },
	{ 0x08,	"Call Confirmed" },
	{ 0x02,	"Call Proceeding" },
	{ 0x07,	"Connect" },
	{ 0x0f,	"Connect Acknowledge" },
	{ 0x0e,	"Emergency Setup" },
	{ 0x03,	"Progress" },
	{ 0x04,	"CC-Establishment" },
	{ 0x06,	"CC-Establishment Confirmed" },
	{ 0x0b,	"Recall" },
	{ 0x09,	"Start CC" },
	{ 0x05,	"Setup" },
	{ 0x17,	"Modify" },
	{ 0x1f,	"Modify Complete" },
	{ 0x13,	"Modify Reject" },
	{ 0x10,	"User Information" },
	{ 0x18,	"Hold" },
	{ 0x19,	"Hold Acknowledge" },
	{ 0x1a,	"Hold Reject" },
	{ 0x1c,	"Retrieve" },
	{ 0x1d,	"Retrieve Acknowledge" },
	{ 0x1e,	"Retrieve Reject" },
	{ 0x25,	"Disconnect" },
	{ 0x2d,	"Release" },
	{ 0x2a,	"Release Complete" },
	{ 0x39,	"Congestion Control" },
	{ 0x3e,	"Notify" },
	{ 0x3d,	"Status" },
	{ 0x34,	"Status Enquiry" },
	{ 0x35,	"Start DTMF" },
	{ 0x31,	"Stop DTMF" },
	{ 0x32,	"Stop DTMF Acknowledge" },
	{ 0x36,	"Start DTMF Acknowledge" },
	{ 0x37,	"Start DTMF Reject" },
	{ 0x3a,	"Facility" },
	{ 0, NULL }
};

const value_string gsm_a_dtap_msg_sms_strings[] = {
	{ 0x01,	"CP-DATA" },
	{ 0x04,	"CP-ACK" },
	{ 0x10,	"CP-ERROR" },
	{ 0, NULL }
};

const value_string gsm_a_dtap_msg_ss_strings[] = {
	{ 0x2a,	"Release Complete" },
	{ 0x3a,	"Facility" },
	{ 0x3b,	"Register" },
	{ 0, NULL }
};

const value_string gsm_a_dtap_msg_tp_strings[] = {
	{ 0x00, "Close TCH Loop Cmd" },
	{ 0x01, "Close TCH Loop Ack" },
	{ 0x06, "Open Loop Cmd" },
	{ 0x0c, "Act EMMI Cmd" },
	{ 0x0d, "Act EMMI Ack" },
	{ 0x10, "Deact EMMI" },
	{ 0x14, "Test Interface" },
	{ 0x20, "Close Multi-slot Loop Cmd" },
	{ 0x21, "Close Multi-slot Loop Ack" },
	{ 0x22, "Open Multi-slot Loop Cmd" },
	{ 0x23, "Open Multi-slot Loop Ack" },
	{ 0x24, "GPRS Test Mode Cmd" },
	{ 0x25, "EGPRS Start Radio Block Loopback Cmd" },
	{ 0x40, "Close UE Test Loop" },
	{ 0x41, "Close UE Test Loop Complete" },
	{ 0x42, "Open UE Test Loop" },
	{ 0x43, "Open UE Test Loop Complete" },
	{ 0x44, "Activate RB Test Mode" },
	{ 0x45, "Activate RB Test Mode Complete" },
	{ 0x46, "Deactivate RB Test Mode" },
	{ 0x47, "Deactivate RB Test Mode Complete" },
	{ 0x48, "Reset UE Positioning Stored Information" },
	{ 0x49, "UE Test Loop Mode 3 RLC SDU Counter Request" },
	{ 0x4A, "UE Test Loop Mode 3 RLC SDU Counter Response" },
	{ 0, NULL }
};

const value_string gsm_dtap_elem_strings[] = {
	/* Mobility Management Information Elements 10.5.3 */
	{ 0x00,	"Authentication Parameter RAND" },
	{ 0x00,	"Authentication Parameter AUTN (UMTS authentication challenge only)" },
	{ 0x00,	"Authentication Response Parameter" },
	{ 0x00,	"Authentication Response Parameter (extension) (UMTS authentication challenge only)" },
	{ 0x00,	"Authentication Failure Parameter (UMTS authentication challenge only)" },
	{ 0x00,	"CM Service Type" },
	{ 0x00,	"Identity Type" },
	{ 0x00,	"Location Updating Type" },
	{ 0x00,	"Network Name" },
	{ 0x00,	"Reject Cause" },
	{ 0x00,	"Follow-on Proceed" },
	{ 0x00,	"Time Zone" },
	{ 0x00,	"Time Zone and Time" },
	{ 0x00,	"CTS Permission" },
	{ 0x00,	"LSA Identifier" },
	{ 0x00,	"Daylight Saving Time" },
	{ 0x00, "Emergency Number List" },
	/* Call Control Information Elements 10.5.4 */
	{ 0x00,	"Auxiliary States" },					/* 10.5.4.4 Auxiliary states */
	{ 0x00,	"Bearer Capability" },					/* 10.5.4.4a Backup bearer capability */
	{ 0x00,	"Call Control Capabilities" },
	{ 0x00,	"Call State" },
	{ 0x00,	"Called Party BCD Number" },
	{ 0x00,	"Called Party Subaddress" },
	{ 0x00,	"Calling Party BCD Number" },
	{ 0x00,	"Calling Party Subaddress" },
	{ 0x00,	"Cause" },
	{ 0x00,	"CLIR Suppression" },
	{ 0x00,	"CLIR Invocation" },
	{ 0x00,	"Congestion Level" },
	{ 0x00,	"Connected Number" },
	{ 0x00,	"Connected Subaddress" },
	{ 0x00,	"Facility" },
	{ 0x00,	"High Layer Compatibility" },
	{ 0x00,	"Keypad Facility" },
	{ 0x00,	"Low Layer Compatibility" },
	{ 0x00,	"More Data" },
	{ 0x00,	"Notification Indicator" },
	{ 0x00,	"Progress Indicator" },
	{ 0x00,	"Recall type $(CCBS)$" },
	{ 0x00,	"Redirecting Party BCD Number" },
	{ 0x00,	"Redirecting Party Subaddress" },
	{ 0x00,	"Repeat Indicator" },
	{ 0x00,	"Reverse Call Setup Direction" },
	{ 0x00,	"SETUP Container $(CCBS)$" },
	{ 0x00,	"Signal" },
	{ 0x00,	"SS Version Indicator" },
	{ 0x00,	"User-user" },
	{ 0x00,	"Alerting Pattern $(NIA)$" },			/* 10.5.4.26 Alerting Pattern $(NIA)$ */
	{ 0x00,	"Allowed Actions $(CCBS)$" },
	{ 0x00,	"Stream Identifier" },
	{ 0x00,	"Network Call Control Capabilities" },
	{ 0x00,	"Cause of No CLI" },
	{ 0x00,	"Immediate Modification Indicator" },	/* 10.5.4.30 Cause of No CLI */
	/* 10.5.4.31 Void */
	{ 0x00,	"Supported Codec List" },				/* 10.5.4.32 Supported codec list */
	{ 0x00,	"Service Category" },					/* 10.5.4.33 Service category */
	/* 10.5.4.34 Redial */
	/* 10.5.4.35 Network-initiated Service Upgrade indicator */
	/* Short Message Service Information Elements [5] 8.1.4 */
	{ 0x00,	"CP-User Data" },
	{ 0x00,	"CP-Cause" },
	/* Short Message Service Information Elements [5] 8.2 */
	{ 0x00,	"RP-Message Reference" },
	{ 0x00,	"RP-Origination Address" },
	{ 0x00,	"RP-Destination Address" },
	{ 0x00,	"RP-User Data" },
	{ 0x00,	"RP-Cause" },
	/* Tests procedures information elements 3GPP TS 44.014 6.4.0 and 3GPP TS 34.109 6.4.0 */
	{ 0x00, "Close TCH Loop Cmd Sub-channel"},
	{ 0x00, "Open Loop Cmd Ack"},
	{ 0x00, "Close Multi-slot Loop Cmd Loop type"},
	{ 0x00, "Close Multi-slot Loop Ack Result"},
	{ 0x00, "Test Interface Tested device"},
	{ 0x00, "GPRS Test Mode Cmd PDU description"},
	{ 0x00, "GPRS Test Mode Cmd Mode flag"},
	{ 0x00, "EGPRS Start Radio Block Loopback Cmd Mode flag"},
	{ 0x00, "Close UE Test Loop Mode"},
	{ 0x00, "UE Positioning Technology"},
	{ 0x00, "RLC SDU Counter Value"},
	{ 0, NULL }
};

const gchar *gsm_a_pd_str[] = {
	"Group Call Control",
	"Broadcast Call Control",
	"Reserved: was allocated in earlier phases of the protocol",
	"Call Control; call related SS messages",
	"GPRS Transparent Transport Protocol (GTTP)",
	"Mobility Management messages",
	"Radio Resources Management messages",
	"Unknown",
	"GPRS Mobility Management messages",
	"SMS messages",
	"GPRS Session Management messages",
	"Non call related SS messages",
	"Location Services",
	"Unknown",
	"Reserved for extension of the PD to one octet length",
	"Special conformance testing functions"
};
/* L3 Protocol discriminator values according to TS 24 007 (6.4.0)  */
const value_string protocol_discriminator_vals[] = {
	{0x0,		"Group call control"},
	{0x1,		"Broadcast call control"},
	{0x2,		"Reserved: was allocated in earlier phases of the protocol"},
	{0x3,		"Call Control; call related SS messages"},
	{0x4,		"GPRS Transparent Transport Protocol (GTTP)"},
	{0x5,		"Mobility Management messages"},
	{0x6,		"Radio Resources Management messages"},
	{0x7,		"Unknown"},
	{0x8,		"GPRS mobility management messages"},
	{0x9,		"SMS messages"},
	{0xa,		"GPRS session management messages"},
	{0xb,		"Non call related SS messages"},
	{0xc,		"Location services specified in 3GPP TS 44.071 [8a]"},
	{0xd,		"Unknown"},
	{0xe,		"Reserved for extension of the PD to one octet length "},
	{0xf,		"Special conformance testing functions"},
	{ 0,	NULL }
};

const value_string gsm_a_pd_short_str_vals[] = {
	{0x0,		"GCC"},				/* Group Call Control */
	{0x1,		"BCC"},				/* Broadcast Call Control */
	{0x2,		"Reserved"},		/* : was allocated in earlier phases of the protocol */
	{0x3,		"CC"},				/* Call Control; call related SS messages */
	{0x4,		"GTTP"},			/* GPRS Transparent Transport Protocol (GTTP) */
	{0x5,		"MM"},				/* Mobility Management messages */
	{0x6,		"RR"},				/* Radio Resources Management messages */
	{0x7,		"Unknown"},
	{0x8,		"GMM"},				/* GPRS Mobility Management messages */
	{0x9,		"SMS"},
	{0xa,		"SM"},				/* GPRS Session Management messages */
	{0xb,		"SS"},
	{0xc,		"LS"},				/* Location Services */
	{0xd,		"Unknown"},
	{0xe,		"Reserved"},		/*  for extension of the PD to one octet length  */
	{0xf,		"TP"},		/*  for tests procedures described in 3GPP TS 44.014 6.4.0 and 3GPP TS 34.109 6.4.0.*/
	{ 0,	NULL }
};


#define	DTAP_PD_MASK		0x0f
#define	DTAP_SKIP_MASK		0xf0
#define	DTAP_TI_MASK		DTAP_SKIP_MASK
#define	DTAP_TIE_PRES_MASK	0x07			/* after TI shifted to right */
#define	DTAP_TIE_MASK		0x7f

#define	DTAP_MM_IEI_MASK	0x3f
#define	DTAP_CC_IEI_MASK	0x3f
#define	DTAP_SMS_IEI_MASK	0xff
#define	DTAP_SS_IEI_MASK	0x3f
#define DTAP_TP_IEI_MASK  0xff

/* Initialize the protocol and registered fields */
static int proto_a_dtap = -1;

static int hf_gsm_a_dtap_msg_mm_type = -1;
static int hf_gsm_a_dtap_msg_cc_type = -1;
static int hf_gsm_a_seq_no = -1;
static int hf_gsm_a_dtap_msg_sms_type = -1;
static int hf_gsm_a_dtap_msg_ss_type = -1;
static int hf_gsm_a_dtap_msg_tp_type = -1;
int hf_gsm_a_dtap_elem_id = -1;
static int hf_gsm_a_cld_party_bcd_num = -1;
static int hf_gsm_a_clg_party_bcd_num = -1;
static int hf_gsm_a_dtap_cause = -1;

int hf_gsm_a_extension = -1;
static int hf_gsm_a_type_of_number = -1;
static int hf_gsm_a_numbering_plan_id = -1;

static int hf_gsm_a_lsa_id = -1;

/* Initialize the subtree pointers */
static gint ett_dtap_msg = -1;
static gint ett_dtap_oct_1 = -1;
static gint ett_cm_srvc_type = -1;
static gint ett_gsm_enc_info = -1;
static gint ett_bc_oct_3a = -1;
static gint ett_bc_oct_4 = -1;
static gint ett_bc_oct_5 = -1;
static gint ett_bc_oct_5a = -1;
static gint ett_bc_oct_5b = -1;
static gint ett_bc_oct_6 = -1;
static gint ett_bc_oct_6a = -1;
static gint ett_bc_oct_6b = -1;
static gint ett_bc_oct_6c = -1;
static gint ett_bc_oct_6d = -1;
static gint ett_bc_oct_6e = -1;
static gint ett_bc_oct_6f = -1;
static gint ett_bc_oct_6g = -1;
static gint ett_bc_oct_7 = -1;

static char a_bigbuf[1024];

static dissector_handle_t data_handle;
static dissector_handle_t gsm_map_handle;
static dissector_handle_t rp_handle;

packet_info *gsm_a_dtap_pinfo;
static proto_tree *g_tree;

/*
 * this should be set on a per message basis, if possible
 */
static gint is_uplink;

#define	NUM_GSM_DTAP_ELEM (sizeof(gsm_dtap_elem_strings)/sizeof(value_string))
gint ett_gsm_dtap_elem[NUM_GSM_DTAP_ELEM];

static dgt_set_t Dgt_mbcd = {
	{
  /*  0   1   2   3   4   5   6   7   8   9   a   b   c   d   e */
	 '0','1','2','3','4','5','6','7','8','9','*','#','a','b','c'
	}
};

/*
 * [3] 10.5.3.1
 */
static guint8
de_auth_param_rand(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;

	len = len;
	curr_offset = offset;

/*
 * 16 octets == 128 bits
 */
#define	AUTH_PARAM_RAND_LEN	16

	proto_tree_add_text(tree,
		tvb, curr_offset, AUTH_PARAM_RAND_LEN,
		"RAND value: %s",
		tvb_bytes_to_str(tvb, curr_offset, AUTH_PARAM_RAND_LEN));

	curr_offset += AUTH_PARAM_RAND_LEN;

	/* no length check possible */

	return(curr_offset - offset);
}

/*
 * [3] 10.5.3.1.1
 */
static guint8
de_auth_param_autn(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;

	curr_offset = offset;

	proto_tree_add_text(tree,
		tvb, curr_offset, len,
		"AUTN value: %s",
		tvb_bytes_to_str(tvb, curr_offset, len));

	curr_offset += len;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.3.2
 */
static guint8
de_auth_resp_param(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;

	len = len;
	curr_offset = offset;

/*
 * 4 octets == 32 bits
 */
#define	AUTH_PARAM_SRES_LEN	4

	proto_tree_add_text(tree,
		tvb, curr_offset, AUTH_PARAM_SRES_LEN,
		"SRES value: %s",
		tvb_bytes_to_str(tvb, curr_offset, AUTH_PARAM_SRES_LEN));

	curr_offset += AUTH_PARAM_SRES_LEN;

	/* no length check possible */

	return(curr_offset - offset);
}

/*
 * [3] 10.5.3.2.1
 */
static guint8
de_auth_resp_param_ext(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;

	curr_offset = offset;

	proto_tree_add_text(tree,
		tvb, curr_offset, len,
		"XRES value: %s",
		tvb_bytes_to_str(tvb, curr_offset, len));

	curr_offset += len;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.3.2.2
 */
static guint8
de_auth_fail_param(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;

	curr_offset = offset;

	proto_tree_add_text(tree,
		tvb, curr_offset, len,
		"AUTS value: %s",
		tvb_bytes_to_str(tvb, curr_offset, len));

	curr_offset += len;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.3.5a
 */
static guint8
de_network_name(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	proto_tree_add_item(tree, hf_gsm_a_extension, tvb, curr_offset, 1, FALSE);

	switch ((oct & 0x70) >> 4)
	{
	case 0x00: str = "Cell Broadcast data coding scheme, GSM default alphabet, language unspecified, defined in 3GPP TS 03.38"; break;
	case 0x01: str = "UCS2 (16 bit)"; break;
	default:
		str = "Reserved";
	break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x70, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Coding Scheme: %s",
		a_bigbuf,
		str);

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Add CI: The MS should %s",
		a_bigbuf,
		(oct & 0x08) ?
			"add the letters for the Country's Initials and a separator (e.g. a space) to the text string" :
			"The MS should not add the letters for the Country's Initials to the text string");

	switch (oct & 0x07)
	{
	case 1: str = "bit 8 is spare and set to '0' in octet n"; break;
	case 2: str = "bits 7 and 8 are spare and set to '0' in octet n"; break;
	case 3: str = "bits 6 to 8(inclusive) are spare and set to '0' in octet n"; break;
	case 4: str = "bits 5 to 8(inclusive) are spare and set to '0' in octet n"; break;
	case 5: str = "bits 4 to 8(inclusive) are spare and set to '0' in octet n"; break;
	case 6: str = "bits 3 to 8(inclusive) are spare and set to '0' in octet n"; break;
	case 7: str = "bits 2 to 8(inclusive) are spare and set to '0' in octet n"; break;
	default:
		str = "this field carries no information about the number of spare bits in octet n";
	break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Number of spare bits in last octet: %s",
		a_bigbuf,
		str);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	proto_tree_add_text(tree,
		tvb, curr_offset, len - 1,
		"Text string encoded according to Coding Scheme");

	curr_offset += len - 1;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/* 3GPP TS 24.008
 * [3] 10.5.3.6 Reject cause
 */
guint8
de_rej_cause(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	switch (oct)
	{
	case 0x02: str = "IMSI unknown in HLR"; break;
	case 0x03: str = "Illegal MS"; break;
	case 0x04: str = "IMSI unknown in VLR"; break;
	case 0x05: str = "IMEI not accepted"; break;
	case 0x06: str = "Illegal ME"; break;
	case 0x0b: str = "PLMN not allowed"; break;
	case 0x0c: str = "Location Area not allowed"; break;
	case 0x0d: str = "Roaming not allowed in this location area"; break;
	case 0x0f: str = "No Suitable Cells In Location Area"; break;
	case 0x11: str = "Network failure"; break;
	case 0x14: str = "MAC failure"; break;
	case 0x15: str = "Synch failure"; break;
	case 0x16: str = "Congestion"; break;
	case 0x17: str = "GSM authentication unacceptable"; break;
	case 0x20: str = "Service option not supported"; break;
	case 0x21: str = "Requested service option not subscribed"; break;
	case 0x22: str = "Service option temporarily out of order"; break;
	case 0x26: str = "Call cannot be identified"; break;
	case 0x5f: str = "Semantically incorrect message"; break;
	case 0x60: str = "Invalid mandatory information"; break;
	case 0x61: str = "Message type non-existent or not implemented"; break;
	case 0x62: str = "Message type not compatible with the protocol state"; break;
	case 0x63: str = "Information element non-existent or not implemented"; break;
	case 0x64: str = "Conditional IE error"; break;
	case 0x65: str = "Message not compatible with the protocol state"; break;
	case 0x6f: str = "Protocol error, unspecified"; break;
	default:
		switch (is_uplink)
		{
		case IS_UPLINK_FALSE:
			str = "Service option temporarily out of order";
			break;
		default:
			str = "Protocol error, unspecified";
			break;
		}
		break;
	}

	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Reject Cause value: 0x%02x (%u) %s",
		oct,
		oct,
		str);

	curr_offset++;

	/* no length check possible */

	return(curr_offset - offset);
}

/*
 * [3] 10.5.3.8
 */
static guint8
de_time_zone(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	char sign;

	curr_offset = offset;

	/* 3GPP TS 23.040 version 6.6.0 Release 6
	 * 9.2.3.11 TP-Service-Centre-Time-Stamp (TP-SCTS)
	 * :
	 * The Time Zone indicates the difference, expressed in quarters of an hour,
	 * between the local time and GMT. In the first of the two semi-octets,
	 * the first bit (bit 3 of the seventh octet of the TP-Service-Centre-Time-Stamp field)
	 * represents the algebraic sign of this difference (0: positive, 1: negative).
	 */

	oct = tvb_get_guint8(tvb, curr_offset);
	sign = (oct & 0x08)?'-':'+';
	oct = (oct >> 4) + (oct & 0x07) * 10;

	proto_tree_add_text(tree,
		tvb, offset, 1,
		"Timezone: GMT %c %d hours %d minutes",
		sign, oct / 4, oct % 4 * 15);
	curr_offset++;

	/* no length check possible */

	return(curr_offset - offset);
}

/*
 * [3] 10.5.3.9
 */
static guint8
de_time_zone_time(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct, oct2, oct3;
	guint32	curr_offset;
	char sign;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);
	oct2 = tvb_get_guint8(tvb, curr_offset+1);
	oct3 = tvb_get_guint8(tvb, curr_offset+2);

	proto_tree_add_text(tree,
		tvb, curr_offset, 3,
		"Year %u%u, Month %u%u, Day %u%u",
		oct & 0x0f,
		(oct & 0xf0) >> 4,
		oct2 & 0x0f,
		(oct2 & 0xf0) >> 4,
		oct3 & 0x0f,
		(oct3 & 0xf0) >> 4);

	curr_offset += 3;

	oct = tvb_get_guint8(tvb, curr_offset);
	oct2 = tvb_get_guint8(tvb, curr_offset+1);
	oct3 = tvb_get_guint8(tvb, curr_offset+2);

	proto_tree_add_text(tree,
		tvb, curr_offset, 3,
		"Hour %u%u, Minutes %u%u, Seconds %u%u",
		oct & 0x0f,
		(oct & 0xf0) >> 4,
		oct2 & 0x0f,
		(oct2 & 0xf0) >> 4,
		oct3 & 0x0f,
		(oct3 & 0xf0) >> 4);

	curr_offset += 3;

	/* 3GPP TS 23.040 version 6.6.0 Release 6
	 * 9.2.3.11 TP-Service-Centre-Time-Stamp (TP-SCTS)
	 * :
	 * The Time Zone indicates the difference, expressed in quarters of an hour,
	 * between the local time and GMT. In the first of the two semi-octets,
	 * the first bit (bit 3 of the seventh octet of the TP-Service-Centre-Time-Stamp field)
	 * represents the algebraic sign of this difference (0: positive, 1: negative).
	 */

	oct = tvb_get_guint8(tvb, curr_offset);
	sign = (oct & 0x08)?'-':'+';
	oct = (oct >> 4) + (oct & 0x07) * 10;

	proto_tree_add_text(tree,
		tvb, offset, 1,
		"Timezone: GMT %c %d hours %d minutes",
		sign, oct / 4, oct % 4 * 15);

	curr_offset++;

	/* no length check possible */

	return(curr_offset - offset);
}

/*
 * [3] 10.5.3.11 3GPP TS 24.008 version 6.8.0 Release 6
 */
static guint8
de_lsa_id(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;

	curr_offset = offset;

	if (len == 0){
		proto_tree_add_text(tree,tvb, curr_offset, len,"LSA ID not included");
	}else{
		proto_tree_add_item(tree, hf_gsm_a_lsa_id, tvb, curr_offset, 3, FALSE);
	}

	curr_offset += len;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.3.12
 */
static guint8
de_day_saving_time(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0xfc, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	switch (oct & 0x03)
	{
	case 0: str = "No adjustment for Daylight Saving Time"; break;
	case 1: str = "+1 hour adjustment for Daylight Saving Time"; break;
	case 2: str = "+2 hours adjustment for Daylight Saving Time"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x03, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  %s",
		a_bigbuf,
		str);

	curr_offset++;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.4.4
 */
static guint8
de_aux_states(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	proto_tree_add_item(tree, hf_gsm_a_extension, tvb, curr_offset, 1, FALSE);

	other_decode_bitfield_value(a_bigbuf, oct, 0x70, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	switch ((oct & 0x0c) >> 2)
	{
	case 0: str = "Idle"; break;
	case 1: str = "Hold request"; break;
	case 2: str = "Call held"; break;
	default:
		str = "Retrieve request";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x0c, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Hold auxiliary state: %s",
		a_bigbuf,
		str);

	switch (oct & 0x03)
	{
	case 0: str = "Idle"; break;
	case 1: str = "MPTY request"; break;
	case 2: str = "Call in MPTY"; break;
	default:
		str = "Split request";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x03, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Multi party auxiliary state: %s",
		a_bigbuf,
		str);

	curr_offset++;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.4.5
 */
guint8
de_bearer_cap(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string, int string_len)
{
	guint8	oct;
	guint8	itc;
	gboolean	extended;
	guint32	curr_offset;
	guint32	saved_offset;
	proto_tree	*subtree;
	proto_item	*item;
	const gchar *str;

#define	DE_BC_ITC_SPEECH	0x00
#define	DE_BC_ITC_UDI		0x01
#define	DE_BC_ITC_EX_PLMN	0x02
#define	DE_BC_ITC_FASC_G3	0x03
#define	DE_BC_ITC_OTHER_ITC	0x05
#define	DE_BC_ITC_RSVD_NET	0x07

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	/* octet 3 */

	/*
	 * warning, bearer cap uses extended values that
	 * are reversed from other parameters!
	 */
	extended = (oct & 0x80) ? FALSE : TRUE;
	itc = oct & 0x07;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	switch (is_uplink)
	{
	case IS_UPLINK_FALSE:
		str = "Spare";
		break;

	case IS_UPLINK_TRUE:
		/*
		 * depends on Information transfer capability
		 */
		switch (itc)
		{
		case DE_BC_ITC_SPEECH:
			if (extended)
			{
				switch ((oct & 0x60) >> 5)
				{
				case 1: str = "MS supports at least full rate speech version 1 but does not support half rate speech version 1"; break;
				case 2: str = "MS supports at least full rate speech version 1 and half rate speech version 1. MS has a greater preference for half rate speech version 1 than for full rate speech version 1"; break;
				case 3: str = "MS supports at least full rate speech version 1 and half rate speech version 1. MS has a greater preference for full rate speech version 1 than for half rate speech version 1"; break;
				default:
					str = "Reserved";
					break;
				}
			}
			else
			{
				switch ((oct & 0x60) >> 5)
				{
				case 1: str = "Full rate support only MS/fullrate speech version 1 supported"; break;
				case 2: str = "Dual rate support MS/half rate speech version 1 preferred, full rate speech version 1 also supported"; break;
				case 3: str = "Dual rate support MS/full rate speech version 1 preferred, half rate speech version 1 also supported"; break;
				default:
					str = "Reserved";
					break;
				}
			}
			break;

		default:
			switch ((oct & 0x60) >> 5)
			{
			case 1: str = "Full rate support only MS"; break;
			case 2: str = "Dual rate support MS/half rate preferred"; break;
			case 3: str = "Dual rate support MS/full rate preferred"; break;
			default:
				str = "Reserved";
				break;
			}
			break;
		}
		break;

		default:
			str = "(dissect problem)";
			break;
		}

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(tree,
	tvb, curr_offset, 1,
	"%s :  Radio channel requirement: %s",
	a_bigbuf,
	str);

	other_decode_bitfield_value(a_bigbuf, oct, 0x10, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Coding standard: %s",
		a_bigbuf,
		(oct & 0x10) ? "reserved" : "GSM standardized coding");

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Transfer mode: %s",
		a_bigbuf,
		(oct & 0x08) ? "packet" : "circuit");

	switch (itc)
	{
	case DE_BC_ITC_SPEECH: str = "Speech"; break;
	case DE_BC_ITC_UDI: str = "Unrestricted digital information"; break;
	case DE_BC_ITC_EX_PLMN: str = "3.1 kHz audio, ex PLMN"; break;
	case DE_BC_ITC_FASC_G3: str = "Facsimile group 3"; break;
	case DE_BC_ITC_OTHER_ITC: str = "Other ITC (See Octet 5a)"; break;
	case DE_BC_ITC_RSVD_NET: str = "Reserved, to be used in the network"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Information transfer capability: %s",
		a_bigbuf,
		str);

	if (add_string)
		g_snprintf(add_string, string_len, " - (%s)", str);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	switch (itc)
	{
	case DE_BC_ITC_SPEECH:
		/* octets 3a */

		item =
			proto_tree_add_text(tree,
			tvb, curr_offset, -1,
			"Octets 3a - Speech Versions");

		subtree = proto_item_add_subtree(item, ett_bc_oct_3a);

		saved_offset = curr_offset;

		do
		{
			oct = tvb_get_guint8(tvb, curr_offset);

			extended = (oct & 0x80) ? FALSE : TRUE;

			other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
			proto_tree_add_text(subtree,
				tvb, curr_offset, 1,
				"%s :  Extension: %s",
				a_bigbuf,
				extended ? "extended" : "not extended");

			other_decode_bitfield_value(a_bigbuf, oct, 0x40, 8);
			proto_tree_add_text(subtree,
				tvb, curr_offset, 1,
				"%s :  Coding: octet used for %s",
				a_bigbuf,
				(oct & 0x40) ? "other extension of octet 3" :
				"extension of information transfer capability");

			other_decode_bitfield_value(a_bigbuf, oct, 0x30, 8);
			proto_tree_add_text(subtree,
				tvb, curr_offset, 1,
				"%s :  Spare",
				a_bigbuf);

			switch (oct & 0x0f)
			{
			case 0: str = "GSM full rate speech version 1"; break;
			case 2: str = "GSM full rate speech version 2"; break;
			case 4: str = "GSM full rate speech version 3"; break;
			case 1: str = "GSM half rate speech version 1"; break;
			case 5: str = "GSM half rate speech version 3"; break;
			default:
				str = "Speech version TBD";
				break;
			}

			other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
			proto_tree_add_text(subtree,
				tvb, curr_offset, 1,
				"%s :  Speech version indication: %s",
				a_bigbuf,
				str);

			curr_offset++;
		}
		while (extended &&
			((len - (curr_offset - offset)) > 0));

		proto_item_set_len(item, curr_offset - saved_offset);
		break;

		default:
		/* octet 4 */

		item =
			proto_tree_add_text(tree,
				tvb, curr_offset, 1,
				"Octet 4");

		subtree = proto_item_add_subtree(item, ett_bc_oct_4);

		oct = tvb_get_guint8(tvb, curr_offset);

		extended = (oct & 0x80) ? FALSE : TRUE;

		other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Extension: %s",
			a_bigbuf,
			extended ? "extended" : "not extended");

		other_decode_bitfield_value(a_bigbuf, oct, 0x40, 8);
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Compression: data compression %s%s",
			a_bigbuf,
			(oct & 0x40) ? "" : "not ",
			is_uplink ? "allowed" : "possible");

		switch ((oct & 0x30) >> 4)
		{
		case 0x00: str = "Service data unit integrity"; break;
		case 0x03: str = "Unstructured"; break;
		default:
			str = "Reserved";
			break;
		}

		other_decode_bitfield_value(a_bigbuf, oct, 0x30, 8);
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Structure: %s",
			a_bigbuf,
			str);

		other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Duplex mode: %s",
			a_bigbuf,
			(oct & 0x08) ? "Full" : "Half");

		other_decode_bitfield_value(a_bigbuf, oct, 0x04, 8);
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Configuration: %s",
			a_bigbuf,
			(oct & 0x04) ? "Reserved" : "Point-to-point");

		other_decode_bitfield_value(a_bigbuf, oct, 0x02, 8);
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  NIRR: %s",
			a_bigbuf,
			(oct & 0x02) ?
			"Data up to and including 4.8 kb/s, full rate, non-transparent, 6 kb/s radio interface rate is requested" :
			"No meaning is associated with this value");

		other_decode_bitfield_value(a_bigbuf, oct, 0x01, 8);
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Establishment: %s",
			a_bigbuf,
			(oct & 0x01) ? "Reserved" : "Demand");

		curr_offset++;

	NO_MORE_DATA_CHECK(len);

	/* octet 5 */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 5");

	subtree = proto_item_add_subtree(item, ett_bc_oct_5);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Access Identity: %s",
		a_bigbuf,
		(oct & 0x60) ? "Reserved" : "Octet identifier");

	switch ((oct & 0x18) >> 3)
	{
	case 0x00: str = "No rate adaption"; break;
	case 0x01: str = "V.110, I.460/X.30 rate adaptation"; break;
	case 0x02: str = "ITU-T X.31 flag stuffing"; break;
	default:
		str = "Other rate adaption (see octet 5a)";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x18, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Rate Adaption: %s",
		a_bigbuf,
		str);

	switch (oct & 0x07)
	{
	case 0x01: str = "I.440/450"; break;
	case 0x02: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	case 0x03: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	case 0x04: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	case 0x05: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	case 0x06: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Signalling Access Protocol: %s",
		a_bigbuf,
		str);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	if (!extended) goto bc_octet_6;

	/* octet 5a */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 5a");

	subtree = proto_item_add_subtree(item, ett_bc_oct_5a);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Other ITC: %s",
		a_bigbuf,
		(oct & 0x60) ? "Reserved" : "Restricted digital information");

	switch ((oct & 0x18) >> 3)
	{
	case 0x00: str = "V.120"; break;
	case 0x01: str = "H.223 & H.245"; break;
	case 0x02: str = "PIAFS"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x18, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Other Rate Adaption: %s",
		a_bigbuf,
		str);

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	if (!extended) goto bc_octet_6;

	/* octet 5b */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 5b");

	subtree = proto_item_add_subtree(item, ett_bc_oct_5b);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	other_decode_bitfield_value(a_bigbuf, oct, 0x40, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Rate Adaption Header: %sincluded",
		a_bigbuf,
		(oct & 0x40) ? "" : "not ");

	other_decode_bitfield_value(a_bigbuf, oct, 0x20, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Multiple frame establishment support in data link: %s",
		a_bigbuf,
		(oct & 0x20) ? "Supported" : "Not supported, only UI frames allowed");

	other_decode_bitfield_value(a_bigbuf, oct, 0x10, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Mode of operation: %s",
		a_bigbuf,
		(oct & 0x10) ? "Protocol sensitive" : "Bit transparent");

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Logical link identifier negotiation: %s",
		a_bigbuf,
		(oct & 0x08) ? "Full protocol negotiation" : "Default, LLI=256 only");

	other_decode_bitfield_value(a_bigbuf, oct, 0x04, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Assignor/Assignee: Message originator is '%s'",
		a_bigbuf,
		(oct & 0x04) ? "assignor only" : "default assignee");

	other_decode_bitfield_value(a_bigbuf, oct, 0x02, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  In band/Out of band negotiation: Negotiation is done %s",
		a_bigbuf,
		(oct & 0x02) ?
		"with USER INFORMATION messages on a temporary signalling connection" :
		"in-band using logical link zero");

	other_decode_bitfield_value(a_bigbuf, oct, 0x01, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

bc_octet_6:

	/* octet 6 */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 6");

	subtree = proto_item_add_subtree(item, ett_bc_oct_6);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Layer 1 Identity: %s",
		a_bigbuf,
		((oct & 0x60) == 0x20) ? "Octet identifier" : "Reserved");

	other_decode_bitfield_value(a_bigbuf, oct, 0x1e, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  User information layer 1 protocol: %s",
		a_bigbuf,
		(oct & 0x1e) ? "Reserved" : "Default layer 1 protocol");

	other_decode_bitfield_value(a_bigbuf, oct, 0x01, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Synchronous/asynchronous: %s",
		a_bigbuf,
		(oct & 0x01) ? "Asynchronous" : "Synchronous");

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	if (!extended) goto bc_octet_7;

	/* octet 6a */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 6a");

	subtree = proto_item_add_subtree(item, ett_bc_oct_6a);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	other_decode_bitfield_value(a_bigbuf, oct, 0x40, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Number of Stop Bits: %s",
		a_bigbuf,
		(oct & 0x40) ? "2" : "1");

	other_decode_bitfield_value(a_bigbuf, oct, 0x20, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Negotiation: %s",
		a_bigbuf,
		(oct & 0x20) ? "Reserved" : "In-band negotiation not possible");

	other_decode_bitfield_value(a_bigbuf, oct, 0x10, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Number of data bits excluding parity bit if present: %s",
		a_bigbuf,
		(oct & 0x10) ? "8" : "7");

	switch (oct & 0x0f)
	{
	case 0x01: str = "0.3 kbit/s Recommendation X.1 and V.110"; break;
	case 0x02: str = "1.2 kbit/s Recommendation X.1 and V.110"; break;
	case 0x03: str = "2.4 kbit/s Recommendation X.1 and V.110"; break;
	case 0x04: str = "4.8 kbit/s Recommendation X.1 and V.110"; break;
	case 0x05: str = "9.6 kbit/s Recommendation X.1 and V.110"; break;
	case 0x06: str = "12.0 kbit/s transparent (non compliance with X.1 and V.110)"; break;
	case 0x07: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  User rate: %s",
		a_bigbuf,
		str);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	if (!extended) goto bc_octet_7;

	/* octet 6b */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 6b");

	subtree = proto_item_add_subtree(item, ett_bc_oct_6b);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	switch ((oct & 0x60) >> 5)
	{
	case 0x02: str = "8 kbit/s"; break;
	case 0x03: str = "16 kbit/s"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  V.110/X.30 rate adaptation Intermediate rate: %s",
		a_bigbuf,
		str);

	other_decode_bitfield_value(a_bigbuf, oct, 0x10, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Network independent clock (NIC) on transmission (Tx): %s to send data with network independent clock",
		a_bigbuf,
		(oct & 0x10) ? "requires" : "does not require");

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Network independent clock (NIC) on reception (Rx): %s accept data with network independent clock",
		a_bigbuf,
		(oct & 0x08) ? "can" : "cannot");

	switch (oct & 0x07)
	{
	case 0x00: str = "Odd"; break;
	case 0x02: str = "Even"; break;
	case 0x03: str = "None"; break;
	case 0x04: str = "Forced to 0"; break;
	case 0x05: str = "Forced to 1"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Parity information: %s",
		a_bigbuf,
		str);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	if (!extended) goto bc_octet_7;

	/* octet 6c */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 6c");

	subtree = proto_item_add_subtree(item, ett_bc_oct_6c);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	switch ((oct & 0x60) >> 5)
	{
	case 0x01: str = "Non transparent (RLP)"; break;
	case 0x02: str = "Both, transparent preferred"; break;
	case 0x03: str = "Both, non transparent preferred"; break;
	default:
		str = "Transparent";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Connection element: %s",
		a_bigbuf,
		str);

	switch (oct & 0x1f)
	{
	case 0x00: str = "None"; break;
	case 0x01: str = "V.21"; break;
	case 0x02: str = "V.22"; break;
	case 0x03: str = "V.22 bis"; break;
	case 0x04: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	case 0x05: str = "V.26 ter"; break;
	case 0x06: str = "V.32"; break;
	case 0x07: str = "Modem for undefined interface"; break;
	case 0x08: str = "Autobauding type 1"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x1f, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Modem type: %s",
		a_bigbuf,
		str);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	if (!extended) goto bc_octet_7;

	/* octet 6d */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 6d");

	subtree = proto_item_add_subtree(item, ett_bc_oct_6d);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	switch ((oct & 0x60) >> 5)
	{
	case 0x00: str = "No other modem type specified in this field"; break;
	case 0x02: str = "V.34"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Other modem type: %s",
		a_bigbuf,
		str);

	switch (oct & 0x1f)
	{
	case 0x00: str = "Fixed network user rate not applicable/No meaning is associated with this value"; break;
	case 0x01: str = "9.6 kbit/s Recommendation X.1 and V.110"; break;
	case 0x02: str = "14.4 kbit/s Recommendation X.1 and V.110"; break;
	case 0x03: str = "19.2 kbit/s Recommendation X.1 and V.110"; break;
	case 0x04: str = "28.8 kbit/s Recommendation X.1 and V.110"; break;
	case 0x05: str = "38.4 kbit/s Recommendation X.1 and V.110"; break;
	case 0x06: str = "48.0 kbit/s Recommendation X.1 and V.110(synch)"; break;
	case 0x07: str = "56.0 kbit/s Recommendation X.1 and V.110(synch) /bit transparent"; break;
	case 0x08: str = "64.0 kbit/s bit transparent"; break;
	case 0x09: str = "33.6 kbit/s bit transparent"; break;
	case 0x0a: str = "32.0 kbit/s Recommendation I.460"; break;
	case 0x0b: str = "31.2 kbit/s Recommendation V.34"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x1f, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Fixed network user rate: %s",
		a_bigbuf,
		str);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	if (!extended) goto bc_octet_7;

	/* octet 6e */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 6e");

	subtree = proto_item_add_subtree(item, ett_bc_oct_6e);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	if (is_uplink == IS_UPLINK_TRUE)
	{
		other_decode_bitfield_value(a_bigbuf, oct, 0x40, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Acceptable channel codings: TCH/F14.4 %sacceptable",
		a_bigbuf,
		(oct & 0x40) ? "" : "not ");

		other_decode_bitfield_value(a_bigbuf, oct, 0x20, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Acceptable channel codings: Spare",
		a_bigbuf);

		other_decode_bitfield_value(a_bigbuf, oct, 0x10, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Acceptable channel codings: TCH/F9.6 %sacceptable",
		a_bigbuf,
		(oct & 0x10) ? "" : "not ");

		other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Acceptable channel codings: TCH/F4.8 %sacceptable",
		a_bigbuf,
		(oct & 0x08) ? "" : "not ");

		other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Maximum number of traffic channels: %u TCH",
		a_bigbuf,
		(oct & 0x07) + 1);
	}
	else
	{
		other_decode_bitfield_value(a_bigbuf, oct, 0x78, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Acceptable channel codings: Spare",
		a_bigbuf);

		other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Maximum number of traffic channels: Spare",
		a_bigbuf);
	}

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	if (!extended) goto bc_octet_7;

	/* octet 6f */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 6f");

	subtree = proto_item_add_subtree(item, ett_bc_oct_6f);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	switch ((oct & 0x70) >> 4)
	{
	case 0x00: str = "not allowed/required/applicable"; break;
	case 0x01: str = "up to 1 TCH/F allowed/may be requested"; break;
	case 0x02: str = "up to 2 TCH/F allowed/may be requested"; break;
	case 0x03: str = "up to 3 TCH/F allowed/may be requested"; break;
	case 0x04: str = "up to 4 TCH/F allowed/may be requested"; break;
	default:
		str = "up to 4 TCH/F may be requested";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x70, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  UIMI, User initiated modification indication: %s",
		a_bigbuf,
		str);

	if (is_uplink == IS_UPLINK_TRUE)
	{
		switch (oct & 0x0f)
		{
		case 0x00: str = "Air interface user rate not applicable/No meaning associated with this value"; break;
		case 0x01: str = "9.6 kbit/s"; break;
		case 0x02: str = "14.4 kbit/s"; break;
		case 0x03: str = "19.2 kbit/s"; break;
		case 0x05: str = "28.8 kbit/s"; break;
		case 0x06: str = "38.4 kbit/s"; break;
		case 0x07: str = "43.2 kbit/s"; break;
		case 0x08: str = "57.6 kbit/s"; break;
		case 0x09: str = "interpreted by the network as 38.4 kbit/s in this version of the protocol"; break;
		case 0x0a: str = "interpreted by the network as 38.4 kbit/s in this version of the protocol"; break;
		case 0x0b: str = "interpreted by the network as 38.4 kbit/s in this version of the protocol"; break;
		case 0x0c: str = "interpreted by the network as 38.4 kbit/s in this version of the protocol"; break;
		default:
		str = "Reserved";
		break;
		}

		other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Wanted air interface user rate: %s",
		a_bigbuf,
		str);
	}
	else
	{
		other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Wanted air interface user rate: Spare",
		a_bigbuf);
	}

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	if (!extended) goto bc_octet_7;

	/* octet 6g */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 6g");

	subtree = proto_item_add_subtree(item, ett_bc_oct_6g);

	oct = tvb_get_guint8(tvb, curr_offset);

	extended = (oct & 0x80) ? FALSE : TRUE;

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	if (is_uplink == IS_UPLINK_TRUE)
	{
		other_decode_bitfield_value(a_bigbuf, oct, 0x40, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Acceptable channel codings extended: TCH/F28.8 %sacceptable",
		a_bigbuf,
		(oct & 0x40) ? "" : "not ");

		other_decode_bitfield_value(a_bigbuf, oct, 0x20, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Acceptable channel codings extended: TCH/F32.0 %sacceptable",
		a_bigbuf,
		(oct & 0x20) ? "" : "not ");

		other_decode_bitfield_value(a_bigbuf, oct, 0x10, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Acceptable channel codings extended: TCH/F43.2 %sacceptable",
		a_bigbuf,
		(oct & 0x10) ? "" : "not ");

		other_decode_bitfield_value(a_bigbuf, oct, 0x10, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Acceptable channel codings extended: TCH/F43.2 %sacceptable",
		a_bigbuf,
		(oct & 0x10) ? "" : "not ");

		switch ((oct & 0x0c) >> 2)
		{
		case 0: str = "Channel coding symmetry preferred"; break;
		case 2: str = "Downlink biased channel coding asymmetry is preferred"; break;
		case 1: str = "Uplink biased channel coding asymmetry is preferred"; break;
		default:
		str = "Unused, treat as Channel coding symmetry preferred";
		break;
		}

		other_decode_bitfield_value(a_bigbuf, oct, 0x0c, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Channel Coding Asymmetry Indication: %s",
		a_bigbuf,
		str);
	}
	else
	{
		other_decode_bitfield_value(a_bigbuf, oct, 0x7c, 8);
		proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  EDGE Channel Codings: Spare",
		a_bigbuf);
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x03, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

bc_octet_7:
	/* octet 7 */

	item =
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Octet 7");

	subtree = proto_item_add_subtree(item, ett_bc_oct_7);
		extended = (oct & 0x80) ? FALSE : TRUE;
		oct = tvb_get_guint8(tvb, curr_offset);
		other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		extended ? "extended" : "not extended");

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Layer 2 Identity: %s",
		a_bigbuf,
		((oct & 0x60) == 0x40) ? "Octet identifier" : "Reserved");

	switch (oct & 0x1f)
	{
	case 0x06: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	case 0x08: str = "ISO 6429, codeset 0 (DC1/DC3)"; break;
	case 0x09: str = "Reserved: was allocated but never used in earlier phases of the protocol"; break;
	case 0x0a: str = "Videotex profile 1"; break;
	case 0x0c: str = "COPnoFlCt (Character oriented Protocol with no Flow Control mechanism)"; break;
	case 0x0d: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x1f, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  User information layer 2 protocol: %s",
		a_bigbuf,
		str);
	break;
	}

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.4.5a
 */
guint8
de_bearer_cap_uplink(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string, int string_len)
{
	is_uplink = IS_UPLINK_TRUE;
	return de_bearer_cap(tvb, tree, offset, len, add_string, string_len);

}


static guint8
de_cc_cap(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0xf0, 8);

	switch ((oct & 0xf0) >> 4)
	{
	case 0:
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Maximum number of supported bearers: 1",
		a_bigbuf);
	break;

	default:
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Maximum number of supported bearers: %u",
		a_bigbuf,
		(oct & 0xf0) >> 4);
	break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x0c, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	other_decode_bitfield_value(a_bigbuf, oct, 0x02, 8);
		proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  PCP: the mobile station %s the Prolonged Clearing Procedure",
		a_bigbuf,
		(oct & 0x02) ? "supports" : "does not support");

	other_decode_bitfield_value(a_bigbuf, oct, 0x01, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  DTMF: %s",
		a_bigbuf,
		(oct & 0x01) ?
			"the mobile station supports DTMF as specified in subclause 5.5.7 of TS 24.008" :
			"reserved for earlier versions of the protocol");

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0xf0, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Maximum number of speech bearers: %u",
		a_bigbuf,
		oct & 0x0f);

	curr_offset++;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.4.6
 */
static guint8
de_call_state(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	proto_tree	*subtree;
	proto_item	*item;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	item =
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		gsm_dtap_elem_strings[DE_CALL_STATE].strptr);

	subtree = proto_item_add_subtree(item, ett_gsm_dtap_elem[DE_CALL_STATE]);

	switch ((oct & 0xc0) >> 6)
	{
	case 0: str = "Coding as specified in ITU-T Rec. Q.931"; break;
	case 1: str = "Reserved for other international standards"; break;
	case 2: str = "National standard"; break;
	default:
		str = "Standard defined for the GSM PLMNS";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0xc0, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Coding standard: %s",
		a_bigbuf,
		str);

	switch (oct & 0x3f)
	{
	case 0x00: str = "UO - null                                 NO - null"; break;
	case 0x02: str = "U0.1- MM connection pending               N0.1- MM connection pending"; break;
	case 0x22: str = "U0.2- CC prompt present                   N0.2- CC connection pending"; break;
	case 0x23: str = "U0.3- Wait for network information        N0.3- Network answer pending"; break;
	case 0x24: str = "U0.4- CC-Establishment present            N0.4- CC-Establishment present"; break;
	case 0x25: str = "U0.5- CC-Establishment confirmed          N0.5- CC-Establishment confirmed"; break;
	case 0x26: str = "U0.6- Recall present                      N0.6- Recall present"; break;
	case 0x01: str = "U1 - call initiated                       N1 - call initiated"; break;
	case 0x03: str = "U3 - mobile originating call proceeding   N3 - mobile originating call proceeding"; break;
	case 0x04: str = "U4 - call delivered                       N4 - call delivered"; break;
	case 0x06: str = "U6 - call present                         N6 - call present"; break;
	case 0x07: str = "U7 - call received                        N7 - call received"; break;
	case 0x08: str = "U8 - connect request                      N8 - connect request"; break;
	case 0x09: str = "U9 - mobile terminating call confirmed    N9 - mobile terminating call confirmed"; break;
	case 0x0a: str = "U10- active                               N10- active"; break;
	case 0x0b: str = "U11- disconnect request"; break;
	case 0x0c: str = "U12- disconnect indication                N12-disconnect indication"; break;
	case 0x13: str = "U19- release request                      N19- release request"; break;
	case 0x1a: str = "U26- mobile originating modify            N26- mobile originating modify"; break;
	case 0x1b: str = "U27- mobile terminating modify            N27- mobile terminating modify"; break;
	case 0x1c: str = "                                          N28- connect indication"; break;
	default:
		str = "Unknown";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x3f, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Call state value: %s",
		a_bigbuf,
		str);

	curr_offset++;

	/* no length check possible */

	return(curr_offset - offset);
}

static const true_false_string gsm_a_extension_value = {
	"No Extension",
	"Extension"
};

const value_string gsm_a_type_of_number_values[] = {
	{ 0x00,	"unknown" },
	{ 0x01,	"International Number" },
	{ 0x02,	"National number" },
	{ 0x03,	"Network Specific Number" },
	{ 0x04,	"Dedicated access, short code" },
	{ 0x05,	"Reserved" },
	{ 0x06,	"Reserved" },
	{ 0x07,	"Reserved for extension" },
	{ 0, NULL }
};

const value_string gsm_a_numbering_plan_id_values[] = {
	{ 0x00,	"unknown" },
	{ 0x01,	"ISDN/Telephony Numbering (Rec ITU-T E.164)" },
	{ 0x02,	"spare" },
	{ 0x03,	"Data Numbering (ITU-T Rec. X.121)" },
	{ 0x04,	"Telex Numbering (ITU-T Rec. F.69)" },
	{ 0x08,	"National Numbering" },
	{ 0x09,	"Private Numbering" },
	{ 0x0d,	"reserved for CTS (see 3GPP TS 44.056 [91])" },
	{ 0x0f,	"Reserved for extension" },
	{ 0, NULL }
};

/*
 * [3] 10.5.4.7
 */
guint8
de_cld_party_bcd_num(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string, int string_len)
{
	guint8	*poctets;
	guint32	curr_offset;

	curr_offset = offset;

	proto_tree_add_item(tree, hf_gsm_a_extension, tvb, curr_offset, 1, FALSE);
	proto_tree_add_item(tree, hf_gsm_a_type_of_number , tvb, curr_offset, 1, FALSE);
	proto_tree_add_item(tree, hf_gsm_a_numbering_plan_id , tvb, curr_offset, 1, FALSE);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	poctets = tvb_get_ephemeral_string(tvb, curr_offset, len - (curr_offset - offset));

	my_dgt_tbcd_unpack(a_bigbuf, poctets, len - (curr_offset - offset),
		&Dgt_mbcd);

	proto_tree_add_string_format(tree, hf_gsm_a_cld_party_bcd_num,
		tvb, curr_offset, len - (curr_offset - offset),
		a_bigbuf,
		"BCD Digits: %s",
		a_bigbuf);

	if (sccp_assoc && ! sccp_assoc->called_party) {
		sccp_assoc->called_party = se_strdup(a_bigbuf);
	}

	curr_offset += len - (curr_offset - offset);

	if (add_string)
		g_snprintf(add_string, string_len, " - (%s)", a_bigbuf);

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.4.8
 */
static guint8
de_cld_party_sub_addr(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	proto_tree_add_item(tree, hf_gsm_a_extension, tvb, curr_offset, 1, FALSE);

	switch ((oct & 0x70) >> 4)
	{
	case 0: str = "NSAP (X.213/ISO 8348 AD2)"; break;
	case 2: str = "User specified"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x70, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Type of subaddress: %s",
		a_bigbuf,
		str);

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Odd/Even indicator: %s",
		a_bigbuf,
		(oct & 0x08) ?
			"odd number of address signals" : "even number of address signals");

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	proto_tree_add_text(tree,
		tvb, curr_offset, len - (curr_offset - offset),
		"Subaddress information");

	curr_offset += len - (curr_offset - offset);

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/* 3GPP TS 24.008
 * [3] 10.5.4.9
 */
static guint8
de_clg_party_bcd_num(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string, int string_len)
{
	guint8	oct;
	guint8	*poctets;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	proto_tree_add_item(tree, hf_gsm_a_extension, tvb, curr_offset, 1, FALSE);
	proto_tree_add_item(tree, hf_gsm_a_type_of_number , tvb, curr_offset, 1, FALSE);
	proto_tree_add_item(tree, hf_gsm_a_numbering_plan_id , tvb, curr_offset, 1, FALSE);

	curr_offset++;

	oct = tvb_get_guint8(tvb, curr_offset);

	proto_tree_add_item(tree, hf_gsm_a_extension, tvb, curr_offset, 1, FALSE);

	switch ((oct & 0x60) >> 5)
	{
	case 0: str = "Presentation allowed"; break;
	case 1: str = "Presentation restricted"; break;
	case 2: str = "Number not available due to interworking"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Presentation indicator: %s",
		a_bigbuf,
		str);

	other_decode_bitfield_value(a_bigbuf, oct, 0x1c, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	switch (oct & 0x03)
	{
	case 0: str = "User-provided, not screened"; break;
	case 1: str = "User-provided, verified and passed"; break;
	case 2: str = "User-provided, verified and failed"; break;
	default:
		str = "Network provided";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x03, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Screening indicator: %s",
		a_bigbuf,
		str);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	poctets = tvb_get_ephemeral_string(tvb, curr_offset, len - (curr_offset - offset));

	my_dgt_tbcd_unpack(a_bigbuf, poctets, len - (curr_offset - offset),
		&Dgt_mbcd);

	proto_tree_add_string_format(tree, hf_gsm_a_clg_party_bcd_num,
		tvb, curr_offset, len - (curr_offset - offset),
		a_bigbuf,
		"BCD Digits: %s",
		a_bigbuf);

	curr_offset += len - (curr_offset - offset);

	if (add_string)
	g_snprintf(add_string, string_len, " - (%s)", a_bigbuf);

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.4.10
 */
static guint8
de_clg_party_sub_addr(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	proto_tree_add_item(tree, hf_gsm_a_extension, tvb, curr_offset, 1, FALSE);

	switch ((oct & 0x70) >> 4)
	{
	case 0: str = "NSAP (X.213/ISO 8348 AD2)"; break;
	case 2: str = "User specified"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x70, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Type of subaddress: %s",
		a_bigbuf,
		str);

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Odd/Even indicator: %s",
		a_bigbuf,
		(oct & 0x08) ?
			"odd number of address signals" : "even number of address signals");

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	curr_offset++;

	NO_MORE_DATA_CHECK(len);

	proto_tree_add_text(tree,
		tvb, curr_offset, len - (curr_offset - offset),
		"Subaddress information");

	curr_offset += len - (curr_offset - offset);

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.4.11
 */
static guint8
de_cause(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string, int string_len)
{
	guint8	oct;
	guint8	cause;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		(oct & 0x80) ? "not extended" : "extended");

	switch ((oct & 0x60) >> 5)
	{
	case 0: str = "Coding as specified in ITU-T Rec. Q.931"; break;
	case 1: str = "Reserved for other international standards"; break;
	case 2: str = "National standard"; break;
	default:
		str = "Standard defined for the GSM PLMNS";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(tree,
	tvb, curr_offset, 1,
	"%s :  Coding standard: %s",
	a_bigbuf,
	str);

	other_decode_bitfield_value(a_bigbuf, oct, 0x10, 8);
	proto_tree_add_text(tree,
	tvb, curr_offset, 1,
	"%s :  Spare",
	a_bigbuf);

	switch (oct & 0x0f)
	{
	case 0: str = "User"; break;
	case 1: str = "Private network serving the local user"; break;
	case 2: str = "Public network serving the local user"; break;
	case 3: str = "Transit network"; break;
	case 4: str = "Public network serving the remote user"; break;
	case 5: str = "Private network serving the remote user"; break;
	case 7: str = "International network"; break;
	case 10: str = "Network beyond interworking point"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Location: %s",
		a_bigbuf,
		str);

	curr_offset++;

	oct = tvb_get_guint8(tvb, curr_offset);

	if (!(oct & 0x80))
	{
	proto_tree_add_item(tree, hf_gsm_a_extension, tvb, curr_offset, 1, FALSE);

	other_decode_bitfield_value(a_bigbuf, oct, 0x7f, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Recommendation",
		a_bigbuf);

	curr_offset++;

	oct = tvb_get_guint8(tvb, curr_offset);
	}

	proto_tree_add_item(tree, hf_gsm_a_extension, tvb, curr_offset, 1, FALSE);

	cause = oct & 0x7f;
	switch (cause)
	{
	case 1: str = "Unassigned (unallocated) number"; break;
	case 3: str = "No route to destination"; break;
	case 6: str = "Channel unacceptable"; break;
	case 8: str = "Operator determined barring"; break;
	case 16: str = "Normal call clearing"; break;
	case 17: str = "User busy"; break;
	case 18: str = "No user responding"; break;
	case 19: str = "User alerting, no answer"; break;
	case 21: str = "Call rejected"; break;
	case 22: str = "Number changed"; break;
	case 25: str = "Pre-emption"; break;
	case 26: str = "Non selected user clearing"; break;
	case 27: str = "Destination out of order"; break;
	case 28: str = "Invalid number format (incomplete number)"; break;
	case 29: str = "Facility rejected"; break;
	case 30: str = "Response to STATUS ENQUIRY"; break;
	case 31: str = "Normal, unspecified"; break;
	case 34: str = "No circuit/channel available"; break;
	case 38: str = "Network out of order"; break;
	case 41: str = "Temporary failure"; break;
	case 42: str = "Switching equipment congestion"; break;
	case 43: str = "Access information discarded"; break;
	case 44: str = "requested circuit/channel not available"; break;
	case 47: str = "Resources unavailable, unspecified"; break;
	case 49: str = "Quality of service unavailable"; break;
	case 50: str = "Requested facility not subscribed"; break;
	case 55: str = "Incoming calls barred within the CUG"; break;
	case 57: str = "Bearer capability not authorized"; break;
	case 58: str = "Bearer capability not presently available"; break;
	case 63: str = "Service or option not available, unspecified"; break;
	case 65: str = "Bearer service not implemented"; break;
	case 68: str = "ACM equal to or greater than ACMmax"; break;
	case 69: str = "Requested facility not implemented"; break;
	case 70: str = "Only restricted digital information bearer capability is available"; break;
	case 79: str = "Service or option not implemented, unspecified"; break;
	case 81: str = "Invalid transaction identifier value"; break;
	case 87: str = "User not member of CUG"; break;
	case 88: str = "Incompatible destination"; break;
	case 91: str = "Invalid transit network selection"; break;
	case 95: str = "Semantically incorrect message"; break;
	case 96: str = "Invalid mandatory information"; break;
	case 97: str = "Message type non-existent or not implemented"; break;
	case 98: str = "Message type not compatible with protocol state"; break;
	case 99: str = "Information element non-existent or not implemented"; break;
	case 100: str = "Conditional IE error"; break;
	case 101: str = "Message not compatible with protocol state"; break;
	case 102: str = "Recovery on timer expiry"; break;
	case 111: str = "Protocol error, unspecified"; break;
	case 127: str = "Interworking, unspecified"; break;
	default:
		if (cause <= 31) { str = "Treat as Normal, unspecified"; }
		else if ((cause >= 32) && (cause <= 47)) { str = "Treat as Resources unavailable, unspecified"; }
		else if ((cause >= 48) && (cause <= 63)) { str = "Treat as Service or option not available, unspecified"; }
		else if ((cause >= 64) && (cause <= 79)) { str = "Treat as Service or option not implemented, unspecified"; }
		else if ((cause >= 80) && (cause <= 95)) { str = "Treat as Semantically incorrect message"; }
		else if ((cause >= 96) && (cause <= 111)) { str = "Treat as Protocol error, unspecified"; }
		else if ((cause >= 112) && (cause <= 127)) { str = "Treat as Interworking, unspecified"; }
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x7f, 8);
	proto_tree_add_uint_format(tree, hf_gsm_a_dtap_cause,
		tvb, curr_offset, 1, cause,
		"%s :  Cause: (%u) %s",
		a_bigbuf,
		cause,
		str);

	curr_offset++;

	if (add_string)
		g_snprintf(add_string, string_len, " - (%u) %s", cause, str);

	NO_MORE_DATA_CHECK(len);

	proto_tree_add_text(tree,
		tvb, curr_offset, len - (curr_offset - offset),
		"Diagnostics");

	curr_offset += len - (curr_offset - offset);

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * 10.5.4.18 Low layer compatibility
 */
static guint8
de_llc(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;

	curr_offset = offset;

	dissect_q931_bearer_capability_ie(tvb, offset, len, tree);

	curr_offset = curr_offset + len;
	return(curr_offset - offset);
}

/*
 * [6] 3.6
 */


static guint8
de_facility(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint fac_len, gchar *add_string _U_, int string_len _U_)
{
	guint	saved_offset;
	gint8 class;
	gboolean pc;
	gboolean ind = FALSE;
	guint32 component_len = 0;
	guint32 header_end_offset;
	guint32 header_len;
	asn1_ctx_t asn1_ctx;
	tvbuff_t *SS_tvb = NULL;
	void *save_private_data;
	static gint comp_type_tag;

	asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, TRUE, gsm_a_dtap_pinfo);

	save_private_data= gsm_a_dtap_pinfo->private_data;
	saved_offset = offset;
	gsm_a_dtap_pinfo->private_data = NULL;
	while ( fac_len > (offset - saved_offset)){

		/* Get the length of the component there can be more than one component in a facility message */

		header_end_offset = get_ber_identifier(tvb, offset, &class, &pc, &comp_type_tag);
		header_end_offset = get_ber_length(tvb, header_end_offset, &component_len, &ind);
		if (ind){
			proto_tree_add_text(tree, tvb, offset+1, 1,
				"Indefinte length, ignoring component");
			return (fac_len);
		}
		header_len = header_end_offset - offset;
		component_len = header_len + component_len;
		/*
		dissect_ROS_Component(FALSE, tvb, offset, &asn1_ctx, tree, hf_ROS_component);
		TODO Call gsm map here
		*/
		SS_tvb = tvb_new_subset(tvb, offset, component_len, component_len);
		call_dissector(gsm_map_handle, SS_tvb, gsm_a_dtap_pinfo, tree);
		offset = offset + component_len;
	}
	gsm_a_dtap_pinfo->private_data = save_private_data;
	return(fac_len);
}

/*
 * [3] 10.5.4.17
 */
static guint8
de_keypad_facility(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string, int string_len)
{
	guint8	oct;
	guint32	curr_offset;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	other_decode_bitfield_value(a_bigbuf, oct, 0x7f, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Keypad information: %c",
		a_bigbuf,
		oct & 0x7f);

	curr_offset++;

	if (add_string)
		g_snprintf(add_string, string_len, " - %c", oct & 0x7f);

	/* no length check possible */

	return(curr_offset - offset);
}

/*
 * [3] 10.5.4.21
 */
static guint8
de_prog_ind(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string, int string_len)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		(oct & 0x80) ? "extended" : "not extended");

	switch ((oct & 0x60) >> 5)
	{
	case 0: str = "Coding as specified in ITU-T Rec. Q.931"; break;
	case 1: str = "Reserved for other international standards"; break;
	case 2: str = "National standard"; break;
	default:
		str = "Standard defined for the GSM PLMNS";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x60, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Coding standard: %s",
		a_bigbuf,
		str);

	other_decode_bitfield_value(a_bigbuf, oct, 0x10, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	switch (oct & 0x0f)
	{
	case 0: str = "User"; break;
	case 1: str = "Private network serving the local user"; break;
	case 2: str = "Public network serving the local user"; break;
	case 4: str = "Public network serving the remote user"; break;
	case 5: str = "Private network serving the remote user"; break;
	case 10: str = "Network beyond interworking point"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Location: %s",
		a_bigbuf,
		str);

	curr_offset++;

	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Extension: %s",
		a_bigbuf,
		(oct & 0x80) ? "extended" : "not extended");

	switch (oct & 0x7f)
	{
	case 1: str = "Call is not end-to-end PLMN/ISDN, further call progress information may be available in-band"; break;
	case 2: str = "Destination address in non-PLMN/ISDN"; break;
	case 3: str = "Origination address in non-PLMN/ISDN"; break;
	case 4: str = "Call has returned to the PLMN/ISDN"; break;
	case 8: str = "In-band information or appropriate pattern now available"; break;
	case 32: str = "Call is end-to-end PLMN/ISDN"; break;
	case 64: str = "Queueing"; break;
	default:
		str = "Unspecific";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x7f, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Progress Description: %s (%d)",
		a_bigbuf,
		str,
		oct & 0x7f);

	if (add_string)
		g_snprintf(add_string, string_len, " - %d", oct & 0x7f);

	curr_offset++;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [3] 10.5.4.22
 */
static guint8
de_repeat_ind(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	switch (oct & 0x0f)
	{
	case 1: str = "Circular for successive selection 'mode 1 alternate mode 2'"; break;
	case 2: str = "Support of fallback mode 1 preferred, mode 2 selected if setup of mode 1 fails"; break;
	case 3: str = "Reserved: was allocated in earlier phases of the protocol"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  %s",
		a_bigbuf,
		str);

	curr_offset++;

	/* no length check possible */

	return(curr_offset - offset);
}

/*
 * [6] 3.7.2
 */
static guint8
de_ss_ver_ind(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	switch (oct)
	{
	case 0: str = "Phase 2 service, ellipsis notation, and phase 2 error handling is supported"; break;
	case 1: str = "SS-Protocol version 3 is supported, and phase 2 error handling is supported"; break;
	default:
		str = "Reserved";
		break;
	}

	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s",
		str);

	curr_offset++;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [5] 8.1.4.1 3GPP TS 24.011 version 6.1.0 Release 6
 */
static guint8
de_cp_user_data(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	tvbuff_t	*rp_tvb;

	curr_offset = offset;

	proto_tree_add_text(tree, tvb, curr_offset, len,
		"RPDU (not displayed)");

	/*
	 * dissect the embedded RP message
	 */
	rp_tvb = tvb_new_subset(tvb, curr_offset, len, len);

	call_dissector(rp_handle, rp_tvb, gsm_a_dtap_pinfo, g_tree);

	curr_offset += len;

	EXTRANEOUS_DATA_CHECK(len, curr_offset - offset);

	return(curr_offset - offset);
}

/*
 * [5] 8.1.4.2
 */
static guint8
de_cp_cause(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string, int string_len)
{
	guint8	oct;
	guint32	curr_offset;
	const gchar *str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	switch (oct)
	{
	case 17: str = "Network failure"; break;
	case 22: str = "Congestion"; break;
	case 81: str = "Invalid Transaction Identifier value"; break;
	case 95: str = "Semantically incorrect message"; break;
	case 96: str = "Invalid mandatory information"; break;
	case 97: str = "Message type non-existent or not implemented"; break;
	case 98: str = "Message not compatible with the short message protocol state"; break;
	case 99: str = "Information element non-existent or not implemented"; break;
	case 111: str = "Protocol error, unspecified"; break;
	default:
		str = "Reserved, treat as Protocol error, unspecified";
		break;
	}

	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Cause: (%u) %s",
		oct,
		str);

	curr_offset++;

	if (add_string)
		g_snprintf(add_string, string_len, " - (%u) %s", oct, str);

	/* no length check possible */

	return(curr_offset - offset);
}

static guint8
de_tp_sub_channel(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guchar	oct;
	const gchar	*str;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset) & 0x3f;
	if ((oct & 0x38) == 0x38)
		str = "I";
	else if ((oct & 0x38) == 0x18)
		str = "F";
	else if ((oct & 0x38) == 0x10)
		str = "E";
	else if ((oct & 0x38) == 0x08)
		str = "D";
	else if ((oct & 0x3c) == 0x04)
		str = "C";
	else if ((oct & 0x3e) == 0x02)
		str = "B";
	else if ((oct & 0x3e) == 0x00)
		str = "A";
	else
		str = "unknown";

	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"Test Loop %s",str);

	if (oct & 0x01)
		proto_tree_add_text(tree,
			tvb, curr_offset, 1,
			"Only one TCH active or sub-channel 0 of two half rate channels is to be looped");
	else
		proto_tree_add_text(tree,
			tvb, curr_offset, 1,
			"Sub-channel 1 of two half rate channels is to be looped");

	curr_offset+= 1;

	return(curr_offset - offset);
}

static guint8
de_tp_ack(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guchar	oct;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	if ((oct & 0xF0) == 0x80)
		proto_tree_add_text(tree,tvb, curr_offset, 1, "Acknowledgment element: %d",oct&0x01);
	else
		proto_tree_add_text(tree,tvb, curr_offset, 1, "No acknowledgment element present");

	curr_offset+= 1;

	return(curr_offset - offset);
}

static guint8
de_tp_loop_type(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guchar	oct;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	switch (oct & 0x03)
	{
		case 0x00:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Channel coding not needed. The Burst-by-Burst loop is activated, type G");
			break;
		case 0x01:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Channel coding needed. Frame erasure is to be signalled, type H");
			break;
		default:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Channel coding reserved (%d)",oct & 0x03);
			break;
	}

	switch (oct & 0x1c)
	{
		case 0x00:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Multi-slot mechanism 1");
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Timeslot number %d",(oct & 0xe0)>>5);
			break;
		case 0x04:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Multi-slot mechanism 2");
			break;
		default:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Loop mechanism reserved (%d)",(oct & 0x1c)>>2);
			break;
	}

	curr_offset+= 1;

	return(curr_offset - offset);
}

static guint8
de_tp_loop_ack(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guchar	oct;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	switch (oct & 0x30)
	{
		case 0x00:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Channel coding not needed. The Burst-by-Burst loop is activated, type G");
			break;
		case 0x10:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Channel coding needed. Frame erasure is to be signalled, type H");
			break;
		default:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Channel coding reserved (%d)",(oct & 0x30)>>4);
			break;
	}

	switch (oct & 0x0e)
	{
		case 0x00:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Multi-slot mechanism 1");
			break;
		case 0x02:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Multi-slot mechanism 2");
			break;
		default:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Loop mechanism reserved (%d)",(oct & 0x0e)>>1);
			break;
	}

	if (oct & 0x01)
		proto_tree_add_text(tree, tvb, curr_offset, 1, "Multi-slot TCH loop was not closed due to error");
	else
		proto_tree_add_text(tree, tvb, curr_offset, 1, "Multi-slot TCH loop was closed successfully");

	curr_offset+= 1;

	return(curr_offset - offset);
}

static guint8
de_tp_tested_device(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guchar	oct;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	switch (oct)
	{
		case 0:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Normal operation (no tested device via DAI)");
			break;
		case 1:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Test of speech decoder / DTX functions (downlink)");
			break;
		case 2:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Test of speech encoder / DTX functions (uplink)");
			break;
		case 4:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Test of acoustic devices and A/D & D/A");
			break;
		default:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Tested device reserved (%d)",oct);
			break;
	}

	curr_offset+= 1;

	return(curr_offset - offset);
}

static guint8
de_tp_pdu_description(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guint16	value;

	curr_offset = offset;

	value = tvb_get_ntohs(tvb, curr_offset);
	curr_offset += 2;

	if (value & 0x8000)
	{
		if ((value & 0xfff) == 0)
			proto_tree_add_text(tree, tvb, curr_offset, 1, "Infinite number of PDUs to be transmitted in the TBF");
		else
			proto_tree_add_text(tree, tvb, curr_offset, 1, "%d PDUs to be transmitted in the TBF",value & 0xfff);
	}
	else
		proto_tree_add_text(tree, tvb, curr_offset, 1, "PDU description reserved");

	return(curr_offset - offset);
}

static guint8
de_tp_mode_flag(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guchar	oct;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	if (oct & 0x01)
		proto_tree_add_text(tree, tvb, curr_offset, 1, "MS shall select the loop back option");
	else
		proto_tree_add_text(tree, tvb, curr_offset, 1, "MS shall itself generate the pseudorandom data");

	proto_tree_add_text(tree, tvb, curr_offset, 1, "Downlink Timeslot Offset: timeslot number %d",(oct & 0x0e)>>1);

	curr_offset+= 1;

	return(curr_offset - offset);
}

static guint8
de_tp_egprs_mode_flag(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guchar	oct;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	if (oct & 0x01)
		proto_tree_add_text(tree, tvb, curr_offset, 1, "MS loops back blocks on the uplink using GMSK modulation only");
	else
		proto_tree_add_text(tree, tvb, curr_offset, 1, "MS loops back blocks on the uplink using either GMSK or 8-PSK modulation following the detected received modulation");

	proto_tree_add_text(tree, tvb, curr_offset, 1, "Downlink Timeslot Offset: timeslot number %d",(oct & 0x0e)>>1);

	curr_offset+= 1;

	return(curr_offset - offset);
}

static guint8
de_tp_ue_test_loop_mode(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guchar	oct;
	guint8	lb_setup_length,i,j;
	guint16 value;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);
	curr_offset+= 1;

	switch (oct & 0x03)
	{
		case 0:
		{
			proto_tree_add_text(tree, tvb, curr_offset, 1, "UE test loop mode 1 loop back (loopback of RLC SDUs or PDCP SDUs)");
			lb_setup_length = tvb_get_guint8(tvb, curr_offset);
			curr_offset += 1;
			for (i=0,j=0; (i<lb_setup_length) && (j<4); i+=3,j++)
			{
				proto_tree_add_text(tree, tvb, curr_offset, 1, "LB setup RB IE %d",j+1);
				value = tvb_get_ntohs(tvb, curr_offset);
				curr_offset += 2;
				proto_tree_add_text(tree, tvb, curr_offset, 1, "Uplink RLC SDU size is %d bits",value);
				oct = tvb_get_guint8(tvb, curr_offset);
				curr_offset+= 1;
				proto_tree_add_text(tree, tvb, curr_offset, 1, "Radio Bearer %d",oct & 0x1f);
			}
			break;
		}
		case 1:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "UE test loop mode 2 loop back (loopback of transport block data and CRC bits)");
			break;
		case 2:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "UE test loop mode 3 RLC SDU counting (counting of received RLC SDUs)");
			oct = tvb_get_guint8(tvb, curr_offset);
			curr_offset+= 1;
			proto_tree_add_text(tree, tvb, curr_offset, 1, "MBMS short transmission identity %d",(oct & 0x1f)+1);
			break;
		default:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "UE test loop mode reserved (%d)",oct & 0x03);
			break;
	}

	return(curr_offset - offset);
}

static guint8
de_tp_ue_positioning_technology(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guchar	oct;

	curr_offset = offset;

	oct = tvb_get_guint8(tvb, curr_offset);

	switch (oct)
	{
		case 0:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "AGPS");
			break;
		default:
			proto_tree_add_text(tree, tvb, curr_offset, 1, "UE positioning technology reserved (%d)",oct);
			break;
	}

	curr_offset+= 1;

	return(curr_offset - offset);
}

static guint8
de_tp_rlc_sdu_counter_value(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len _U_, gchar *add_string _U_, int string_len _U_)
{
	guint32	curr_offset;
	guint32 value;

	curr_offset = offset;

	value = tvb_get_ntohl(tvb, curr_offset);
	curr_offset+= 4;

	proto_tree_add_text(tree, tvb, curr_offset, 1, "UE received RLC SDU counter value %d",value);

	return(curr_offset - offset);
}

guint8 (*dtap_elem_fcn[])(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len, gchar *add_string, int string_len) = {
	/* Mobility Management Information Elements 10.5.3 */
	de_auth_param_rand,	/* Authentication Parameter RAND */
	de_auth_param_autn,	/* Authentication Parameter AUTN (UMTS authentication challenge only) */
	de_auth_resp_param,	/* Authentication Response Parameter */
	de_auth_resp_param_ext,	/* Authentication Response Parameter (extension) (UMTS authentication challenge only) */
	de_auth_fail_param,	/* Authentication Failure Parameter (UMTS authentication challenge only) */
	NULL /* handled inline */,	/* CM Service Type */
	NULL /* handled inline */,	/* Identity Type */
	NULL /* handled inline */,	/* Location Updating Type */
	de_network_name,	/* Network Name */
	de_rej_cause,	/* Reject Cause */
	NULL /* no associated data */,	/* Follow-on Proceed */
	de_time_zone,	/* Time Zone */
	de_time_zone_time,	/* Time Zone and Time */
	NULL /* no associated data */,	/* CTS Permission */
	de_lsa_id,	/* LSA Identifier */
	de_day_saving_time,	/* Daylight Saving Time */
	NULL, /* Emergency Number List */
	/* Call Control Information Elements 10.5.4 */
	de_aux_states,	/* Auxiliary States */
	de_bearer_cap,	/* Bearer Capability */
	de_cc_cap,	/* Call Control Capabilities */
	de_call_state,	/* Call State */
	de_cld_party_bcd_num,	/* Called Party BCD Number */
	de_cld_party_sub_addr,	/* Called Party Subaddress */
	de_clg_party_bcd_num,	/* Calling Party BCD Number */
	de_clg_party_sub_addr,	/* Calling Party Subaddress */
	de_cause,	/* Cause */
	NULL /* no associated data */,	/* CLIR Suppression */
	NULL /* no associated data */,	/* CLIR Invocation */
	NULL /* handled inline */,	/* Congestion Level */
	NULL,	/* Connected Number */
	NULL,	/* Connected Subaddress */
	de_facility,	/* Facility */
	NULL,	/* High Layer Compatibility */
	de_keypad_facility,	/* Keypad Facility */
	de_llc,							/* 10.5.4.18 Low layer compatibility */
	NULL,	/* More Data */
	NULL,	/* Notification Indicator */
	de_prog_ind,	/* Progress Indicator */
	NULL,	/* Recall type $(CCBS)$ */
	NULL,	/* Redirecting Party BCD Number */
	NULL,	/* Redirecting Party Subaddress */
	de_repeat_ind,	/* Repeat Indicator */
	NULL /* no associated data */,	/* Reverse Call Setup Direction */
	NULL,	/* SETUP Container $(CCBS)$ */
	NULL,	/* Signal */
	de_ss_ver_ind,	/* SS Version Indicator */
	NULL,	/* User-user */
	NULL,	/* Alerting Pattern $(NIA)$ */
	NULL,	/* Allowed Actions $(CCBS)$ */
	NULL,	/* Stream Identifier */
	NULL,	/* Network Call Control Capabilities */
	NULL,	/* Cause of No CLI */
	NULL,	/* Immediate Modification Indicator */
	NULL,	/* Supported Codec List */
	NULL,	/* Service Category */
	/* Short Message Service Information Elements [5] 8.1.4 */
	de_cp_user_data,	/* CP-User Data */
	de_cp_cause,	/* CP-Cause */
	/* Tests procedures information elements 3GPP TS 44.014 6.4.0 and 3GPP TS 34.109 6.4.0 */
	de_tp_sub_channel,	/* Close TCH Loop Cmd Sub-channel */
	de_tp_ack,	/* Open Loop Cmd Ack */
	de_tp_loop_type,			/* Close Multi-slot Loop Cmd Loop type */
	de_tp_loop_ack,			/* Close Multi-slot Loop Ack Result */
	de_tp_tested_device,			/* Test Interface Tested device */
	de_tp_pdu_description,			/* GPRS Test Mode Cmd PDU description */
	de_tp_mode_flag,			/* GPRS Test Mode Cmd Mode flag */
	de_tp_egprs_mode_flag,			/* EGPRS Start Radio Block Loopback Cmd Mode flag */
	de_tp_ue_test_loop_mode,			/* Close UE Test Loop Mode */
	de_tp_ue_positioning_technology,			/* UE Positioning Technology */
	de_tp_rlc_sdu_counter_value,			/* RLC SDU Counter Value */
	NULL,	/* NONE */
};

/* MESSAGE FUNCTIONS */

/*
 * [4] 9.2.2
 */
static void
dtap_mm_auth_req(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;
	guint8	oct;
	proto_tree	*subtree;
	proto_item	*item;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	/*
	 * special dissection for Cipher Key Sequence Number
	 */
	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0xf0, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	item =
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		gsm_common_elem_strings[DE_CIPH_KEY_SEQ_NUM].strptr);

	subtree = proto_item_add_subtree(item, ett_gsm_common_elem[DE_CIPH_KEY_SEQ_NUM]);

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);

	switch (oct & 0x07)
	{
	case 0x07:
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Ciphering Key Sequence Number: No key is available",
			a_bigbuf);
		break;

	default:
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Ciphering Key Sequence Number: %u",
			a_bigbuf,
			oct & 0x07);
		break;
	}

	curr_offset++;
	curr_len--;

	if (curr_len <= 0) return;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_AUTH_PARAM_RAND);

	ELEM_OPT_TLV(0x20, GSM_A_PDU_TYPE_DTAP, DE_AUTH_PARAM_AUTN, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.2.3
 */
static void
dtap_mm_auth_resp(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_AUTH_RESP_PARAM);

	ELEM_OPT_TLV(0x21, GSM_A_PDU_TYPE_DTAP, DE_AUTH_RESP_PARAM_EXT, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.2.3a
 */
static void
dtap_mm_auth_fail(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_REJ_CAUSE);

	ELEM_OPT_TLV(0x22, GSM_A_PDU_TYPE_DTAP, DE_AUTH_FAIL_PARAM, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.4
 */
static void
dtap_mm_cm_reestab_req(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;
	guint8	oct;
	proto_tree	*subtree;
	proto_item	*item;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	/*
	 * special dissection for Cipher Key Sequence Number
	 */
	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0xf0, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	item =
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		gsm_common_elem_strings[DE_CIPH_KEY_SEQ_NUM].strptr);

	subtree = proto_item_add_subtree(item, ett_gsm_common_elem[DE_CIPH_KEY_SEQ_NUM]);

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);

	switch (oct & 0x07)
	{
	case 0x07:
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Ciphering Key Sequence Number: No key is available",
			a_bigbuf);
		break;

	default:
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Ciphering Key Sequence Number: %u",
			a_bigbuf,
			oct & 0x07);
		break;
	}

	curr_offset++;
	curr_len--;

	if (curr_len <= 0) return;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_COMMON, DE_MS_CM_2, "");

	ELEM_MAND_LV(GSM_A_PDU_TYPE_COMMON, DE_MID, "");

	ELEM_OPT_TV(0x13, GSM_A_PDU_TYPE_COMMON, DE_LAI, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.5a
 */
static void
dtap_mm_cm_srvc_prompt(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_COMMON, DE_PD_SAPI);

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.2.6
 */
static void
dtap_mm_cm_srvc_rej(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_REJ_CAUSE);

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.2.8
 */
static void
dtap_mm_abort(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_REJ_CAUSE);

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.9
 */
static void
dtap_mm_cm_srvc_req(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;
	guint8	oct;
	proto_tree	*subtree;
	proto_item	*item;
	const gchar *str;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	/*
	 * special dissection for CM Service Type
	 */
	oct = tvb_get_guint8(tvb, curr_offset);

	item =
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		gsm_common_elem_strings[DE_CIPH_KEY_SEQ_NUM].strptr);

	subtree = proto_item_add_subtree(item, ett_gsm_common_elem[DE_CIPH_KEY_SEQ_NUM]);

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	other_decode_bitfield_value(a_bigbuf, oct, 0x70, 8);

	switch ((oct & 0x70) >> 4)
	{
	case 0x07:
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Ciphering Key Sequence Number: No key is available",
			a_bigbuf);
		break;

	default:
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Ciphering Key Sequence Number: %u",
			a_bigbuf,
			(oct & 0x70) >> 4);
		break;
	}

	item =
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		gsm_dtap_elem_strings[DE_CM_SRVC_TYPE].strptr);

	subtree = proto_item_add_subtree(item, ett_gsm_dtap_elem[DE_CM_SRVC_TYPE]);

	switch (oct & 0x0f)
	{
	case 0x01: str = "Mobile originating call establishment or packet mode connection establishment"; break;
	case 0x02: str = "Emergency call establishment"; break;
	case 0x04: str = "Short message service"; break;
	case 0x08: str = "Supplementary service activation"; break;
	case 0x09: str = "Voice group call establishment"; break;
	case 0x0a: str = "Voice broadcast call establishment"; break;
	case 0x0b: str = "Location Services"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Service Type: (%u) %s",
		a_bigbuf,
		oct & 0x0f,
		str);

	curr_offset++;
	curr_len--;

	if (curr_len <= 0) return;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_COMMON, DE_MS_CM_2, "");

	ELEM_MAND_LV(GSM_A_PDU_TYPE_COMMON, DE_MID, "");

	ELEM_OPT_TV_SHORT(0x80, GSM_A_PDU_TYPE_COMMON, DE_PRIO, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.10
 */
static void
dtap_mm_id_req(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint8	oct;
	guint32	curr_offset;
	guint	curr_len;
	proto_tree	*subtree;
	proto_item	*item;
	const gchar *str;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	/*
	 * special dissection for Identity Type
	 */
	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0xf0, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	item =
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		gsm_dtap_elem_strings[DE_ID_TYPE].strptr);

	subtree = proto_item_add_subtree(item, ett_gsm_dtap_elem[DE_ID_TYPE]);

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	switch (oct & 0x07)
	{
	case 1: str = "IMSI"; break;
	case 2: str = "IMEI"; break;
	case 3: str = "IMEISV"; break;
	case 4: str = "TMSI"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x07, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Type of identity: %s",
		a_bigbuf,
		str);

	curr_offset++;
	curr_len--;

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.11
 */
static void
dtap_mm_id_resp(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_COMMON, DE_MID, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.12
 */
static void
dtap_mm_imsi_det_ind(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_COMMON, DE_MS_CM_1);

	ELEM_MAND_LV(GSM_A_PDU_TYPE_COMMON, DE_MID, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.13
 */
static void
dtap_mm_loc_upd_acc(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_COMMON, DE_LAI);

	ELEM_OPT_TLV(0x17, GSM_A_PDU_TYPE_COMMON, DE_MID, "");

	ELEM_OPT_T(0xa1, GSM_A_PDU_TYPE_DTAP, DE_FOP, "");

	ELEM_OPT_T(0xa2, GSM_A_PDU_TYPE_DTAP, DE_CTS_PERM, "");

	ELEM_OPT_TLV(0x4a, GSM_A_PDU_TYPE_COMMON, DE_PLMN_LIST, " Equivalent");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.14
 */
static void
dtap_mm_loc_upd_rej(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_REJ_CAUSE);

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.15
 */
static void
dtap_mm_loc_upd_req(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;
	guint8	oct;
	proto_tree	*subtree;
	proto_item	*item;
	const gchar *str;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	/*
	 * special dissection for Location Updating Type
	 */
	oct = tvb_get_guint8(tvb, curr_offset);

	item =
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		gsm_common_elem_strings[DE_CIPH_KEY_SEQ_NUM].strptr);

	subtree = proto_item_add_subtree(item, ett_gsm_common_elem[DE_CIPH_KEY_SEQ_NUM]);

	other_decode_bitfield_value(a_bigbuf, oct, 0x80, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	other_decode_bitfield_value(a_bigbuf, oct, 0x70, 8);

	switch ((oct & 0x70) >> 4)
	{
	case 0x07:
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Ciphering Key Sequence Number: No key is available",
			a_bigbuf);
		break;

	default:
		proto_tree_add_text(subtree,
			tvb, curr_offset, 1,
			"%s :  Ciphering Key Sequence Number: %u",
			a_bigbuf,
			(oct & 0x70) >> 4);
		break;
	}

	item =
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		gsm_dtap_elem_strings[DE_LOC_UPD_TYPE].strptr);

	subtree = proto_item_add_subtree(item, ett_gsm_dtap_elem[DE_LOC_UPD_TYPE]);

	other_decode_bitfield_value(a_bigbuf, oct, 0x08, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Follow-On Request (FOR): %s",
		a_bigbuf,
		(oct & 0x08) ? "Follow-on request pending" : "No follow-on request pending");

	other_decode_bitfield_value(a_bigbuf, oct, 0x04, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	switch (oct & 0x03)
	{
	case 0: str = "Normal"; break;
	case 1: str = "Periodic"; break;
	case 2: str = "IMSI attach"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x03, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Updating Type: %s",
		a_bigbuf,
		str);

	proto_item_append_text(item, " - %s", str);

	curr_offset++;
	curr_len--;

	if (curr_len <= 0) return;

	ELEM_MAND_V(GSM_A_PDU_TYPE_COMMON, DE_LAI);

	ELEM_MAND_V(GSM_A_PDU_TYPE_COMMON, DE_MS_CM_1);

	ELEM_MAND_LV(GSM_A_PDU_TYPE_COMMON, DE_MID, "");

	ELEM_OPT_TLV(0x33, GSM_A_PDU_TYPE_COMMON, DE_MS_CM_2, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}


/*
 * [4] 9.2.15a
 */
void
dtap_mm_mm_info(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_OPT_TLV(0x43, GSM_A_PDU_TYPE_DTAP, DE_NETWORK_NAME, " - Full Name");

	ELEM_OPT_TLV(0x45, GSM_A_PDU_TYPE_DTAP, DE_NETWORK_NAME, " - Short Name");

	ELEM_OPT_TV(0x46, GSM_A_PDU_TYPE_DTAP, DE_TIME_ZONE, " - Local");

	ELEM_OPT_TV(0x47, GSM_A_PDU_TYPE_DTAP, DE_TIME_ZONE_TIME, " - Universal Time and Local Time Zone");

	ELEM_OPT_TLV(0x48, GSM_A_PDU_TYPE_DTAP, DE_LSA_ID, "");

	ELEM_OPT_TLV(0x49, GSM_A_PDU_TYPE_DTAP, DE_DAY_SAVING_TIME, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.2.16
 */
static void
dtap_mm_mm_status(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_REJ_CAUSE);

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [3] 9.2.17
 */
static void
dtap_mm_tmsi_realloc_cmd(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_COMMON, DE_LAI);

	ELEM_MAND_LV(GSM_A_PDU_TYPE_COMMON, DE_MID, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.1
 */
static void
dtap_cc_alerting(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_OPT_TLV(0x1c, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	ELEM_OPT_TLV(0x1e, GSM_A_PDU_TYPE_DTAP, DE_PROG_IND, "");

	ELEM_OPT_TLV(0x7e, GSM_A_PDU_TYPE_DTAP, DE_USER_USER, "");

	/* uplink only */

	ELEM_OPT_TLV(0x7f, GSM_A_PDU_TYPE_DTAP, DE_SS_VER_IND, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.2
 */
static void
dtap_cc_call_conf(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_OPT_TV_SHORT(0xd0, GSM_A_PDU_TYPE_DTAP, DE_REPEAT_IND, " BC repeat indicator");

	ELEM_OPT_TLV(0x04, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, " 1");

	ELEM_OPT_TLV(0x04, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, " 2");

	ELEM_OPT_TLV(0x08, GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	ELEM_OPT_TLV(0x15, GSM_A_PDU_TYPE_DTAP, DE_CC_CAP, "");

	ELEM_OPT_TLV(0x2d, GSM_A_PDU_TYPE_DTAP, DE_SI, "");

	ELEM_OPT_TLV(0x40, GSM_A_PDU_TYPE_DTAP, DE_SUP_CODEC_LIST, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.3
 */
static void
dtap_cc_call_proceed(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_OPT_TV_SHORT(0xd0, GSM_A_PDU_TYPE_DTAP, DE_REPEAT_IND, " BC repeat indicator");

	ELEM_OPT_TLV(0x04, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, " 1");

	ELEM_OPT_TLV(0x04, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, " 2");

	ELEM_OPT_TLV(0x1c, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	ELEM_OPT_TLV(0x1e, GSM_A_PDU_TYPE_DTAP, DE_PROG_IND, "");

	ELEM_OPT_TV_SHORT(0x80, GSM_A_PDU_TYPE_COMMON, DE_PRIO, "");

	ELEM_OPT_TLV(0x2f, GSM_A_PDU_TYPE_DTAP, DE_NET_CC_CAP, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.4
 */
static void
dtap_cc_congestion_control(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;
	guint8	oct;
	proto_tree	*subtree;
	proto_item	*item;
	const gchar *str;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	/*
	 * special dissection for Congestion Level
	 */
	oct = tvb_get_guint8(tvb, curr_offset);

	other_decode_bitfield_value(a_bigbuf, oct, 0xf0, 8);
	proto_tree_add_text(tree,
		tvb, curr_offset, 1,
		"%s :  Spare",
		a_bigbuf);

	item =
		proto_tree_add_text(tree,
			tvb, curr_offset, 1,
			gsm_dtap_elem_strings[DE_CONGESTION].strptr);

	subtree = proto_item_add_subtree(item, ett_gsm_dtap_elem[DE_CONGESTION]);

	switch (oct & 0x0f)
	{
	case 0: str = "Receiver ready"; break;
	case 15: str = "Receiver not ready"; break;
	default:
		str = "Reserved";
		break;
	}

	other_decode_bitfield_value(a_bigbuf, oct, 0x0f, 8);
	proto_tree_add_text(subtree,
		tvb, curr_offset, 1,
		"%s :  Congestion level: %s",
		a_bigbuf,
		str);

	curr_offset++;
	curr_len--;

	if (curr_len <= 0) return;

	ELEM_OPT_TLV(0x08, GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.5
 */
static void
dtap_cc_connect(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_OPT_TLV(0x1c, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	ELEM_OPT_TLV(0x1e, GSM_A_PDU_TYPE_DTAP, DE_PROG_IND, "");

	ELEM_OPT_TLV(0x4c, GSM_A_PDU_TYPE_DTAP, DE_CONN_NUM, "");

	ELEM_OPT_TLV(0x4d, GSM_A_PDU_TYPE_DTAP, DE_CONN_SUB_ADDR, "");

	ELEM_OPT_TLV(0x7e, GSM_A_PDU_TYPE_DTAP, DE_USER_USER, "");

	/* uplink only */

	ELEM_OPT_TLV(0x7f, GSM_A_PDU_TYPE_DTAP, DE_SS_VER_IND, "");

	ELEM_OPT_TLV(0x2d, GSM_A_PDU_TYPE_DTAP, DE_SI, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.7
 */
static void
dtap_cc_disconnect(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	ELEM_OPT_TLV(0x1c, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	ELEM_OPT_TLV(0x1e, GSM_A_PDU_TYPE_DTAP, DE_PROG_IND, "");

	ELEM_OPT_TLV(0x7e, GSM_A_PDU_TYPE_DTAP, DE_USER_USER, "");

	ELEM_OPT_TLV(0x7b, GSM_A_PDU_TYPE_DTAP, DE_ALLOWED_ACTIONS, "");

	/* uplink only */

	ELEM_OPT_TLV(0x7f, GSM_A_PDU_TYPE_DTAP, DE_SS_VER_IND, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.8
 */
static void
dtap_cc_emerg_setup(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_OPT_TLV(0x04, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, "");

	ELEM_OPT_TLV(0x2d, GSM_A_PDU_TYPE_DTAP, DE_SI, "");

	ELEM_OPT_TLV(0x40, GSM_A_PDU_TYPE_DTAP, DE_SUP_CODEC_LIST, "");

	ELEM_OPT_TLV(0x2e, GSM_A_PDU_TYPE_DTAP, DE_SRVC_CAT, " Emergency");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.9
 */
static void
dtap_cc_facility(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	/* uplink only */

	ELEM_OPT_TLV(0x7f, GSM_A_PDU_TYPE_DTAP, DE_SS_VER_IND, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.12
 */
static void
dtap_cc_hold_rej(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.13
 */
static void
dtap_cc_modify(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, "");

	ELEM_OPT_TLV(0x7c, GSM_A_PDU_TYPE_DTAP, DE_LLC, "");

	ELEM_OPT_TLV(0x7d, GSM_A_PDU_TYPE_DTAP, DE_HLC, "");

	ELEM_OPT_T(0xa3, GSM_A_PDU_TYPE_DTAP, DE_REV_CALL_SETUP_DIR, "");

	ELEM_OPT_T(0xa4, GSM_A_PDU_TYPE_DTAP, DE_IMM_MOD_IND, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.14
 */
static void
dtap_cc_modify_complete(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, "");

	ELEM_OPT_TLV(0x7c, GSM_A_PDU_TYPE_DTAP, DE_LLC, "");

	ELEM_OPT_TLV(0x7d, GSM_A_PDU_TYPE_DTAP, DE_HLC, "");

	ELEM_OPT_T(0xa3, GSM_A_PDU_TYPE_DTAP, DE_REV_CALL_SETUP_DIR, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.15
 */
static void
dtap_cc_modify_rej(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, "");

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	ELEM_OPT_TLV(0x7c, GSM_A_PDU_TYPE_DTAP, DE_LLC, "");

	ELEM_OPT_TLV(0x7d, GSM_A_PDU_TYPE_DTAP, DE_HLC, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.16
 */
static void
dtap_cc_notify(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_NOT_IND);

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.17
 */
static void
dtap_cc_progress(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_PROG_IND, "");

	ELEM_OPT_TLV(0x7e, GSM_A_PDU_TYPE_DTAP, DE_USER_USER, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.17a
 */
static void
dtap_cc_cc_est(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_SETUP_CONTAINER, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.17b
 */
static void
dtap_cc_cc_est_conf(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_OPT_TV_SHORT(0xd0, GSM_A_PDU_TYPE_DTAP, DE_REPEAT_IND, " Repeat indicator");

	ELEM_MAND_TLV(0x04, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, " 1");

	ELEM_OPT_TLV(0x04, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, " 2");

	ELEM_OPT_TLV(0x08, GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	ELEM_OPT_TLV(0x40, GSM_A_PDU_TYPE_DTAP, DE_SUP_CODEC_LIST, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.18
 */
static void
dtap_cc_release(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_OPT_TLV(0x08, GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	ELEM_OPT_TLV(0x08, GSM_A_PDU_TYPE_DTAP, DE_CAUSE, " 2");

	ELEM_OPT_TLV(0x1c, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	ELEM_OPT_TLV(0x7e, GSM_A_PDU_TYPE_DTAP, DE_USER_USER, "");

	/* uplink only */

	ELEM_OPT_TLV(0x7f, GSM_A_PDU_TYPE_DTAP, DE_SS_VER_IND, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.18a
 */
static void
dtap_cc_recall(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_RECALL_TYPE);

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.19
 */
static void
dtap_cc_release_complete(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_OPT_TLV(0x08, GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	ELEM_OPT_TLV(0x1c, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	ELEM_OPT_TLV(0x7e, GSM_A_PDU_TYPE_DTAP, DE_USER_USER, "");

	/* uplink only */

	ELEM_OPT_TLV(0x7f, GSM_A_PDU_TYPE_DTAP, DE_SS_VER_IND, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.22
 */
static void
dtap_cc_retrieve_rej(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.23
 * 3GPP TS 24.008 version 7.5.0 Release 7
 */
static void
dtap_cc_setup(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_OPT_TV_SHORT(0xd0, GSM_A_PDU_TYPE_DTAP, DE_REPEAT_IND, " BC repeat indicator");

	ELEM_OPT_TLV(0x04, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, " 1");

	ELEM_OPT_TLV(0x04, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, " 2");

	ELEM_OPT_TLV(0x1c, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	ELEM_OPT_TLV(0x1e, GSM_A_PDU_TYPE_DTAP, DE_PROG_IND, "");

	ELEM_OPT_TV(0x34, GSM_A_PDU_TYPE_DTAP, DE_SIGNAL, "");

	ELEM_OPT_TLV(0x5c, GSM_A_PDU_TYPE_DTAP, DE_CLG_PARTY_BCD_NUM, "");

	ELEM_OPT_TLV(0x5d, GSM_A_PDU_TYPE_DTAP, DE_CLG_PARTY_SUB_ADDR, "");

	ELEM_OPT_TLV(0x5e, GSM_A_PDU_TYPE_DTAP, DE_CLD_PARTY_BCD_NUM, "");

	ELEM_OPT_TLV(0x6d, GSM_A_PDU_TYPE_DTAP, DE_CLD_PARTY_SUB_ADDR, "");

	ELEM_OPT_TLV(0x74, GSM_A_PDU_TYPE_DTAP, DE_RED_PARTY_BCD_NUM, "");

	ELEM_OPT_TLV(0x75, GSM_A_PDU_TYPE_DTAP, DE_RED_PARTY_SUB_ADDR, "");

	ELEM_OPT_TV_SHORT(0xd0, GSM_A_PDU_TYPE_DTAP, DE_REPEAT_IND, " LLC repeat indicator");

	ELEM_OPT_TLV(0x7c, GSM_A_PDU_TYPE_DTAP, DE_LLC, " 1");

	ELEM_OPT_TLV(0x7c, GSM_A_PDU_TYPE_DTAP, DE_LLC, " 2");

	ELEM_OPT_TV_SHORT(0xd0, GSM_A_PDU_TYPE_DTAP, DE_REPEAT_IND, " HLC repeat indicator");

	ELEM_OPT_TLV(0x7d, GSM_A_PDU_TYPE_DTAP, DE_HLC, " 1");

	ELEM_OPT_TLV(0x7d, GSM_A_PDU_TYPE_DTAP, DE_HLC, " 2");

	ELEM_OPT_TLV(0x7e, GSM_A_PDU_TYPE_DTAP, DE_USER_USER, "");

	/* downlink only */

	ELEM_OPT_TV_SHORT(0x80, GSM_A_PDU_TYPE_COMMON, DE_PRIO, "");

	ELEM_OPT_TLV(0x19, GSM_A_PDU_TYPE_DTAP, DE_ALERT_PATTERN, "");

	ELEM_OPT_TLV(0x2f, GSM_A_PDU_TYPE_DTAP, DE_NET_CC_CAP, "");

	ELEM_OPT_TLV(0x3a, GSM_A_PDU_TYPE_DTAP, DE_CAUSE_NO_CLI, "");

	/* Backup bearer capability O TLV 3-15 10.5.4.4a */
	ELEM_OPT_TLV(0x41, GSM_A_PDU_TYPE_DTAP, DE_BEARER_CAP, "");

	/* uplink only */

	ELEM_OPT_TLV(0x7f, GSM_A_PDU_TYPE_DTAP, DE_SS_VER_IND, "");

	ELEM_OPT_T(0xa1, GSM_A_PDU_TYPE_DTAP, DE_CLIR_SUP, "");

	ELEM_OPT_T(0xa2, GSM_A_PDU_TYPE_DTAP, DE_CLIR_INV, "");

	ELEM_OPT_TLV(0x15, GSM_A_PDU_TYPE_DTAP, DE_CC_CAP, "");

	ELEM_OPT_TLV(0x1d, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, " $(CCBS)$ (advanced recall alignment)");

	ELEM_OPT_TLV(0x1b, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, " (recall alignment Not essential) $(CCBS)$");

	ELEM_OPT_TLV(0x2d, GSM_A_PDU_TYPE_DTAP, DE_SI, "");

	ELEM_OPT_TLV(0x40, GSM_A_PDU_TYPE_DTAP, DE_SUP_CODEC_LIST, "");

	/*A3 Redial Redial O T 1 10.5.4.34
	 * TODO add this element
	 * ELEM_OPT_T(0xA3, GSM_A_PDU_TYPE_DTAP, DE_REDIAL, "");
	 */

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.23a
 */
static void
dtap_cc_start_cc(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_OPT_TLV(0x15, GSM_A_PDU_TYPE_DTAP, DE_CC_CAP, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.24
 */
static void
dtap_cc_start_dtmf(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_TV(0x2c, GSM_A_PDU_TYPE_DTAP, DE_KEYPAD_FACILITY, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.25
 */
static void
dtap_cc_start_dtmf_ack(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_TV(0x2c, GSM_A_PDU_TYPE_DTAP, DE_KEYPAD_FACILITY, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.26
 */
static void
dtap_cc_start_dtmf_rej(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.27
 */
static void
dtap_cc_status(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_FALSE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_CAUSE, "");

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_CALL_STATE);

	ELEM_OPT_TLV(0x24, GSM_A_PDU_TYPE_DTAP, DE_AUX_STATES, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [4] 9.3.31
 */
static void
dtap_cc_user_info(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_USER_USER, "");

	ELEM_OPT_T(0xa0, GSM_A_PDU_TYPE_DTAP, DE_MORE_DATA, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [6] 2.4.2
 */
static void
dtap_ss_register(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_TLV(0x1c, GSM_A_PDU_TYPE_DTAP, DE_FACILITY, "");

	ELEM_OPT_TLV(0x7f, GSM_A_PDU_TYPE_DTAP, DE_SS_VER_IND, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [5] 7.2.1
 */
static void
dtap_sms_cp_data(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_LV(GSM_A_PDU_TYPE_DTAP, DE_CP_USER_DATA, "");

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

/*
 * [5] 7.2.3
 */
static void
dtap_sms_cp_error(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_offset = offset;
	curr_len = len;

	is_uplink = IS_UPLINK_TRUE;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_CP_CAUSE);

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_close_tch_loop_cmd(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_SUB_CHANNEL );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_open_loop_cmd(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	if (curr_len)
		ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_ACK );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_multi_slot_loop_cmd(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_LOOP_TYPE );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_multi_slot_loop_ack(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_LOOP_ACK );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_test_interface(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_TESTED_DEVICE );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_gprs_test_mode_cmd(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_PDU_DESCRIPTION );

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_MODE_FLAG );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_egprs_start_radio_block_loopback_cmd(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_EGPRS_MODE_FLAG );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_close_ue_test_loop(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_UE_TEST_LOOP_MODE );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_reset_ue_positioning_ue_stored_information(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_UE_POSITIONING_TECHNOLOGY );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

static void
dtap_tp_ue_test_loop_mode_3_rlc_sdu_counter_response(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len)
{
	guint32	curr_offset;
	guint32	consumed;
	guint	curr_len;

	curr_len = len;
	curr_offset = offset;

	ELEM_MAND_V(GSM_A_PDU_TYPE_DTAP, DE_TP_RLC_SDU_COUNTER_VALUE );

	EXTRANEOUS_DATA_CHECK(curr_len, 0);
}

#define	NUM_GSM_DTAP_MSG_MM (sizeof(gsm_a_dtap_msg_mm_strings)/sizeof(value_string))
static gint ett_gsm_dtap_msg_mm[NUM_GSM_DTAP_MSG_MM];
static void (*dtap_msg_mm_fcn[])(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len) = {
	dtap_mm_imsi_det_ind,	/* IMSI Detach Indication */
	dtap_mm_loc_upd_acc,	/* Location Updating Accept */
	dtap_mm_loc_upd_rej,	/* Location Updating Reject */
	dtap_mm_loc_upd_req,	/* Location Updating Request */
	NULL /* no associated data */,	/* Authentication Reject */
	dtap_mm_auth_req,	/* Authentication Request */
	dtap_mm_auth_resp,	/* Authentication Response */
	dtap_mm_auth_fail,	/* Authentication Failure */
	dtap_mm_id_req,	/* Identity Request */
	dtap_mm_id_resp,	/* Identity Response */
	dtap_mm_tmsi_realloc_cmd,	/* TMSI Reallocation Command */
	NULL /* no associated data */,	/* TMSI Reallocation Complete */
	NULL /* no associated data */,	/* CM Service Accept */
	dtap_mm_cm_srvc_rej,	/* CM Service Reject */
	NULL /* no associated data */,	/* CM Service Abort */
	dtap_mm_cm_srvc_req,	/* CM Service Request */
	dtap_mm_cm_srvc_prompt,	/* CM Service Prompt */
	NULL,	/* Reserved: was allocated in earlier phases of the protocol */
	dtap_mm_cm_reestab_req,	/* CM Re-establishment Request */
	dtap_mm_abort,	/* Abort */
	NULL /* no associated data */,	/* MM Null */
	dtap_mm_mm_status,	/* MM Status */
	dtap_mm_mm_info,	/* MM Information */
	NULL,	/* NONE */
};

#define	NUM_GSM_DTAP_MSG_CC (sizeof(gsm_a_dtap_msg_cc_strings)/sizeof(value_string))
static gint ett_gsm_dtap_msg_cc[NUM_GSM_DTAP_MSG_CC];
static void (*dtap_msg_cc_fcn[])(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len) = {
	dtap_cc_alerting,	/* Alerting */
	dtap_cc_call_conf,	/* Call Confirmed */
	dtap_cc_call_proceed,	/* Call Proceeding */
	dtap_cc_connect,	/* Connect */
	NULL /* no associated data */,	/* Connect Acknowledge */
	dtap_cc_emerg_setup,	/* Emergency Setup */
	dtap_cc_progress,	/* Progress */
	dtap_cc_cc_est,	/* CC-Establishment */
	dtap_cc_cc_est_conf,	/* CC-Establishment Confirmed */
	dtap_cc_recall,	/* Recall */
	dtap_cc_start_cc,	/* Start CC */
	dtap_cc_setup,	/* Setup */
	dtap_cc_modify,	/* Modify */
	dtap_cc_modify_complete,	/* Modify Complete */
	dtap_cc_modify_rej,	/* Modify Reject */
	dtap_cc_user_info,	/* User Information */
	NULL /* no associated data */,	/* Hold */
	NULL /* no associated data */,	/* Hold Acknowledge */
	dtap_cc_hold_rej,	/* Hold Reject */
	NULL /* no associated data */,	/* Retrieve */
	NULL /* no associated data */,	/* Retrieve Acknowledge */
	dtap_cc_retrieve_rej,	/* Retrieve Reject */
	dtap_cc_disconnect,	/* Disconnect */
	dtap_cc_release,	/* Release */
	dtap_cc_release_complete,	/* Release Complete */
	dtap_cc_congestion_control,	/* Congestion Control */
	dtap_cc_notify,	/* Notify */
	dtap_cc_status,	/* Status */
	NULL /* no associated data */,	/* Status Enquiry */
	dtap_cc_start_dtmf,	/* Start DTMF */
	NULL /* no associated data */,	/* Stop DTMF */
	NULL /* no associated data */,	/* Stop DTMF Acknowledge */
	dtap_cc_start_dtmf_ack,	/* Start DTMF Acknowledge */
	dtap_cc_start_dtmf_rej,	/* Start DTMF Reject */
	dtap_cc_facility,	/* Facility */
	NULL,	/* NONE */
};

#define	NUM_GSM_DTAP_MSG_SMS (sizeof(gsm_a_dtap_msg_sms_strings)/sizeof(value_string))
static gint ett_gsm_dtap_msg_sms[NUM_GSM_DTAP_MSG_SMS];
static void (*dtap_msg_sms_fcn[])(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len) = {
	dtap_sms_cp_data,	/* CP-DATA */
	NULL /* no associated data */,	/* CP-ACK */
	dtap_sms_cp_error,	/* CP-ERROR */
	NULL,	/* NONE */
};

#define	NUM_GSM_DTAP_MSG_SS (sizeof(gsm_a_dtap_msg_ss_strings)/sizeof(value_string))
static gint ett_gsm_dtap_msg_ss[NUM_GSM_DTAP_MSG_SS];
static void (*dtap_msg_ss_fcn[])(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len) = {
	dtap_cc_release_complete,	/* Release Complete */
	dtap_cc_facility,	/* Facility */
	dtap_ss_register,	/* Register */
	NULL,	/* NONE */
};

#define	NUM_GSM_DTAP_MSG_TP (sizeof(gsm_a_dtap_msg_tp_strings)/sizeof(value_string))
static gint ett_gsm_dtap_msg_tp[NUM_GSM_DTAP_MSG_TP];
static void (*dtap_msg_tp_fcn[])(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len) = {
	dtap_tp_close_tch_loop_cmd,	/* CLOSE TCH LOOP CMD */
	NULL,	/* CLOSE TCH LOOP ACK */
	dtap_tp_open_loop_cmd,	/* OPEN LOOP CMD */
	NULL,	/* ACT EMMI CMD */
	NULL,	/* ACT EMMI ACK */
	NULL,	/* DEACT EMMI */
	dtap_tp_test_interface,	/* Test Interface */
	dtap_tp_multi_slot_loop_cmd,	/* CLOSE Multi-slot LOOP CMD */
	dtap_tp_multi_slot_loop_ack,	/* CLOSE Multi-slot LOOP ACK */
	NULL,	/* OPEN Multi-slot LOOP CMD */
	NULL,	/* OPEN Multi-slot LOOP ACK */
	dtap_tp_gprs_test_mode_cmd,	/* GPRS TEST MODE CMD */
	dtap_tp_egprs_start_radio_block_loopback_cmd,	/* EGPRS START RADIO BLOCK LOOPBACK CMD */
	dtap_tp_close_ue_test_loop,	/* CLOSE UE TEST LOOP */
	NULL,	/* CLOSE UE TEST LOOP COMPLETE */
	NULL,	/* OPEN UE TEST LOOP */
	NULL,	/* OPEN UE TEST LOOP COMPLETE */
	NULL,	/* ACTIVATE RB TEST MODE */
	NULL,	/* ACTIVATE RB TEST MODE COMPLETE */
	NULL,	/* DEACTIVATE RB TEST MODE */
	NULL,	/* DEACTIVATE RB TEST MODE COMPLETE */
	dtap_tp_reset_ue_positioning_ue_stored_information,	/* RESET UE POSITIONING STORED INFORMATION */
	NULL,	/* UE Test Loop Mode 3 RLC SDU Counter Request */
	dtap_tp_ue_test_loop_mode_3_rlc_sdu_counter_response,	/* UE Test Loop Mode 3 RLC SDU Counter Response */
	NULL,	/* NONE */
};

/* GENERIC DISSECTOR FUNCTIONS */

static void
dissect_dtap(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	static gsm_a_tap_rec_t	tap_rec[4];
	static gsm_a_tap_rec_t	*tap_p;
	static guint			tap_current=0;
	void			(*msg_fcn)(tvbuff_t *tvb, proto_tree *tree, guint32 offset, guint len);
	guint8			oct;
	guint8			pd;
	guint32			offset;
	guint32			len;
	guint32			oct_1, oct_2;
	gint			idx;
	proto_item			*dtap_item = NULL;
	proto_tree			*dtap_tree = NULL;
	proto_item			*oct_1_item = NULL;
	proto_tree			*pd_tree = NULL;
	const gchar			*msg_str;
	gint			ett_tree;
	gint			ti;
	int				hf_idx;
	gboolean			nsd;


	len = tvb_length(tvb);

	if (len < 2)
	{
		/*
		 * too short to be DTAP
		 */
		call_dissector(data_handle, tvb, pinfo, tree);
		return;
	}

	if (check_col(pinfo->cinfo, COL_INFO))
	{
		col_append_str(pinfo->cinfo, COL_INFO, "(DTAP) ");
	}

	/*
	 * set tap record pointer
	 */
	tap_current++;
	if (tap_current >= 4)
	{
		tap_current = 0;
	}
	tap_p = &tap_rec[tap_current];


	offset = 0;
	oct_2 = 0;

	gsm_a_dtap_pinfo = pinfo;
	g_tree = tree;

	/*
	 * get protocol discriminator
	 */
	oct_1 = tvb_get_guint8(tvb, offset++);

	if ((((oct_1 & DTAP_TI_MASK) >> 4) & DTAP_TIE_PRES_MASK) == DTAP_TIE_PRES_MASK)
	{
		/*
		 * eventhough we don't know if a TI should be in the message yet
		 * we rely on the TI/SKIP indicator to be 0 to avoid taking this
		 * octet
		 */
		oct_2 = tvb_get_guint8(tvb, offset++);
	}

	oct = tvb_get_guint8(tvb, offset);

	pd = oct_1 & DTAP_PD_MASK;
	ti = -1;
	msg_str = NULL;
	ett_tree = -1;
	hf_idx = -1;
	msg_fcn = NULL;
	nsd = FALSE;
	if (check_col(pinfo->cinfo, COL_INFO))
	{
		col_append_fstr(pinfo->cinfo, COL_INFO, "(%s) ",val_to_str(pd,gsm_a_pd_short_str_vals,"unknown"));
	}

	/*
	 * octet 1
	 */
	switch (pd)
	{
	case 3:
		msg_str = match_strval_idx((guint32) (oct & DTAP_CC_IEI_MASK), gsm_a_dtap_msg_cc_strings, &idx);
		ett_tree = ett_gsm_dtap_msg_cc[idx];
		hf_idx = hf_gsm_a_dtap_msg_cc_type;
		msg_fcn = dtap_msg_cc_fcn[idx];
		ti = (oct_1 & DTAP_TI_MASK) >> 4;
		nsd = TRUE;
		break;

	case 5:
		msg_str = match_strval_idx((guint32) (oct & DTAP_MM_IEI_MASK), gsm_a_dtap_msg_mm_strings, &idx);
		ett_tree = ett_gsm_dtap_msg_mm[idx];
		hf_idx = hf_gsm_a_dtap_msg_mm_type;
		msg_fcn = dtap_msg_mm_fcn[idx];
		nsd = TRUE;
		break;

	case 6:
		get_rr_msg_params(oct, &msg_str, &ett_tree, &hf_idx, &msg_fcn);
		break;

	case 8:
		get_gmm_msg_params(oct, &msg_str, &ett_tree, &hf_idx, &msg_fcn);
		break;

	case 9:
		msg_str = match_strval_idx((guint32) (oct & DTAP_SMS_IEI_MASK), gsm_a_dtap_msg_sms_strings, &idx);
		ett_tree = ett_gsm_dtap_msg_sms[idx];
		hf_idx = hf_gsm_a_dtap_msg_sms_type;
		msg_fcn = dtap_msg_sms_fcn[idx];
		ti = (oct_1 & DTAP_TI_MASK) >> 4;
		break;

	case 10:
		get_sm_msg_params(oct, &msg_str, &ett_tree, &hf_idx, &msg_fcn);
		ti = (oct_1 & DTAP_TI_MASK) >> 4;
		break;

	case 11:
		msg_str = match_strval_idx((guint32) (oct & DTAP_SS_IEI_MASK), gsm_a_dtap_msg_ss_strings, &idx);
		ett_tree = ett_gsm_dtap_msg_ss[idx];
		hf_idx = hf_gsm_a_dtap_msg_ss_type;
		msg_fcn = dtap_msg_ss_fcn[idx];
		ti = (oct_1 & DTAP_TI_MASK) >> 4;
		nsd = TRUE;
		break;

	case 15:
		msg_str = match_strval_idx((guint32) (oct & DTAP_TP_IEI_MASK), gsm_a_dtap_msg_tp_strings, &idx);
		ett_tree = ett_gsm_dtap_msg_tp[idx];
		hf_idx = hf_gsm_a_dtap_msg_tp_type;
		msg_fcn = dtap_msg_tp_fcn[idx];
		ti = (oct_1 & DTAP_TI_MASK) >> 4;
		nsd = TRUE;
		break;

	default:
		/* XXX - hf_idx is still -1! this is a bug in the implementation, and I don't know how to fix it so simple return here */
		return;
	}

	sccp_msg = pinfo->sccp_info;

	if (sccp_msg && sccp_msg->data.co.assoc) {
		sccp_assoc = sccp_msg->data.co.assoc;
	} else {
		sccp_assoc = NULL;
		sccp_msg = NULL;
	}

	/*
	 * create the protocol tree
	 */
	if (msg_str == NULL)
	{
		dtap_item =
			proto_tree_add_protocol_format(tree, proto_a_dtap, tvb, 0, len,
			"GSM A-I/F DTAP - Unknown DTAP Message Type (0x%02x)",
			oct);

		dtap_tree = proto_item_add_subtree(dtap_item, ett_dtap_msg);

		if (sccp_msg && !sccp_msg->data.co.label) {
			sccp_msg->data.co.label = se_strdup_printf("DTAP (0x%02x)",oct);
		}


	}
	else
	{
		dtap_item =
			proto_tree_add_protocol_format(tree, proto_a_dtap, tvb, 0, -1,
				"GSM A-I/F DTAP - %s",
				msg_str);

		dtap_tree = proto_item_add_subtree(dtap_item, ett_tree);

		if (sccp_msg && !sccp_msg->data.co.label) {
			sccp_msg->data.co.label = se_strdup(msg_str);
		}

		if (check_col(pinfo->cinfo, COL_INFO))
		{
			col_append_fstr(pinfo->cinfo, COL_INFO, "%s ", msg_str);
		}
	}

	oct_1_item =
	proto_tree_add_text(dtap_tree,
		tvb, 0, 1,
		"Protocol Discriminator: %s",
		val_to_str(pd, protocol_discriminator_vals, "Unknown (%u)"));

	pd_tree = proto_item_add_subtree(oct_1_item, ett_dtap_oct_1);

	if (ti == -1)
	{
		proto_tree_add_item(pd_tree, hf_gsm_a_skip_ind, tvb, 0, 1, FALSE);
	}
	else
	{
		other_decode_bitfield_value(a_bigbuf, oct_1, 0x80, 8);
		proto_tree_add_text(pd_tree,
			tvb, 0, 1,
			"%s :  TI flag: %s",
			a_bigbuf,
			((oct_1 & 0x80) ?  "allocated by receiver" : "allocated by sender"));

		if ((ti & DTAP_TIE_PRES_MASK) == DTAP_TIE_PRES_MASK)
		{
			/* ti is extended to next octet */

			other_decode_bitfield_value(a_bigbuf, oct_1, 0x70, 8);
			proto_tree_add_text(pd_tree,
				tvb, 0, 1,
				"%s :  TIO: The TI value is given by the TIE in octet 2",
				a_bigbuf);
		}
		else
		{
			other_decode_bitfield_value(a_bigbuf, oct_1, 0x70, 8);
			proto_tree_add_text(pd_tree,
				tvb, 0, 1,
				"%s :  TIO: %u",
				a_bigbuf,
				ti & DTAP_TIE_PRES_MASK);
		}
	}

	proto_tree_add_item(pd_tree, hf_gsm_a_L3_protocol_discriminator, tvb, 0, 1, FALSE);

	if ((ti != -1) &&
		(ti & DTAP_TIE_PRES_MASK) == DTAP_TIE_PRES_MASK)
	{
		proto_tree_add_item(tree, hf_gsm_a_extension, tvb, 1, 1, FALSE);

		other_decode_bitfield_value(a_bigbuf, oct_2, DTAP_TIE_MASK, 8);
		proto_tree_add_text(pd_tree,
			tvb, 1, 1,
			"%s :  TIE: %u",
			a_bigbuf,
			oct_2 & DTAP_TIE_MASK);
	}

	/*
	 * N(SD)
	 */
	if ((pinfo->p2p_dir == P2P_DIR_RECV) &&
		nsd)
	{
		/* XXX */
	}
	/* In case of Mobility Management and Call Control and Call related SS messages
	 * bit 7 and 8 is sequence number
	 */
	if((pd==5)||(pd==3)){
		proto_tree_add_item(dtap_tree, hf_gsm_a_seq_no, tvb, offset, 1, FALSE);
	}
	/*
	 * add DTAP message name
	 */
	proto_tree_add_item(dtap_tree, hf_idx, tvb, offset, 1, FALSE);
	offset++;

	tap_p->pdu_type = GSM_A_PDU_TYPE_DTAP;
	tap_p->message_type = (nsd ? (oct & 0x3f) : oct);
	tap_p->protocol_disc = pd;

	tap_queue_packet(gsm_a_tap, pinfo, tap_p);

	if (msg_str == NULL) return;

	if ((len - offset) <= 0) return;

	/*
	 * decode elements
	 */
	if (msg_fcn == NULL)
	{
		proto_tree_add_text(dtap_tree,
			tvb, offset, len - offset,
			"Message Elements");
	}
	else
	{
		(*msg_fcn)(tvb, dtap_tree, offset, len - offset);
	}
}


/* Register the protocol with Wireshark */
void
proto_register_gsm_a_dtap(void)
{
	guint		i;
	guint		last_offset;

	/* Setup list of header fields */

	static hf_register_info hf[] =
	{
	{ &hf_gsm_a_seq_no,
		{ "Sequence number", "gsm_a.dtap_seq_no",
		FT_UINT8, BASE_DEC, NULL, 0xc0,
		"", HFILL }
	},
	{ &hf_gsm_a_dtap_msg_mm_type,
		{ "DTAP Mobility Management Message Type", "gsm_a.dtap_msg_mm_type",
		FT_UINT8, BASE_HEX, VALS(gsm_a_dtap_msg_mm_strings), 0x3f,
		"", HFILL }
	},
	{ &hf_gsm_a_dtap_msg_cc_type,
		{ "DTAP Call Control Message Type", "gsm_a.dtap_msg_cc_type",
		FT_UINT8, BASE_HEX, VALS(gsm_a_dtap_msg_cc_strings), 0x3f,
		"", HFILL }
	},
	{ &hf_gsm_a_dtap_msg_sms_type,
		{ "DTAP Short Message Service Message Type", "gsm_a.dtap_msg_sms_type",
		FT_UINT8, BASE_HEX, VALS(gsm_a_dtap_msg_sms_strings), 0x0,
		"", HFILL }
	},
	{ &hf_gsm_a_dtap_msg_ss_type,
		{ "DTAP Non call Supplementary Service Message Type", "gsm_a.dtap_msg_ss_type",
		FT_UINT8, BASE_HEX, VALS(gsm_a_dtap_msg_ss_strings), 0x0,
		"", HFILL }
	},
	{ &hf_gsm_a_dtap_msg_tp_type,
		{ "DTAP Tests Procedures Message Type", "gsm_a.dtap_msg_tp_type",
		FT_UINT8, BASE_HEX, VALS(gsm_a_dtap_msg_tp_strings), 0x0,
		"", HFILL }
	},
	{ &hf_gsm_a_dtap_elem_id,
		{ "Element ID", "gsm_a_dtap.elem_id",
		FT_UINT8, BASE_DEC, NULL, 0,
		"", HFILL }
	},
	{ &hf_gsm_a_cld_party_bcd_num,
		{ "Called Party BCD Number", "gsm_a.cld_party_bcd_num",
		FT_STRING, BASE_DEC, 0, 0,
		"", HFILL }
	},
	{ &hf_gsm_a_clg_party_bcd_num,
		{ "Calling Party BCD Number", "gsm_a.clg_party_bcd_num",
		FT_STRING, BASE_DEC, 0, 0,
		"", HFILL }
	},
	{ &hf_gsm_a_dtap_cause,
		{ "DTAP Cause", "gsm_a_dtap.cause",
		FT_UINT8, BASE_HEX, 0, 0x0,
		"", HFILL }
	},
	{ &hf_gsm_a_extension,
		{ "Extension", "gsm_a.extension",
		FT_BOOLEAN, 8, TFS(&gsm_a_extension_value), 0x80,
		"Extension", HFILL }
	},
	{ &hf_gsm_a_type_of_number,
		{ "Type of number", "gsm_a.type_of_number",
		FT_UINT8, BASE_HEX, VALS(gsm_a_type_of_number_values), 0x70,
		"Type of number", HFILL }
	},
	{ &hf_gsm_a_numbering_plan_id,
		{ "Numbering plan identification", "gsm_a.numbering_plan_id",
		FT_UINT8, BASE_HEX, VALS(gsm_a_numbering_plan_id_values), 0x0f,
		"Numbering plan identification", HFILL }
	},
	{ &hf_gsm_a_lsa_id,
		{ "LSA Identifier", "gsm_a.lsa_id",
		FT_UINT24, BASE_HEX, NULL, 0x0,
		"LSA Identifier", HFILL }
	},
	};

	/* Setup protocol subtree array */
#define	NUM_INDIVIDUAL_ELEMS	18
	static gint *ett[NUM_INDIVIDUAL_ELEMS +
			NUM_GSM_DTAP_MSG_MM + NUM_GSM_DTAP_MSG_CC +
			NUM_GSM_DTAP_MSG_SMS + NUM_GSM_DTAP_MSG_SS + NUM_GSM_DTAP_MSG_TP +
			NUM_GSM_DTAP_ELEM];

	ett[0] = &ett_dtap_msg;
	ett[1] = &ett_dtap_oct_1;
	ett[2] = &ett_cm_srvc_type;
	ett[3] = &ett_gsm_enc_info;
	ett[4] = &ett_bc_oct_3a;
	ett[5] = &ett_bc_oct_4;
	ett[6] = &ett_bc_oct_5;
	ett[7] = &ett_bc_oct_5a;
	ett[8] = &ett_bc_oct_5b;
	ett[9] = &ett_bc_oct_6;
	ett[10] = &ett_bc_oct_6a;
	ett[11] = &ett_bc_oct_6b;
	ett[12] = &ett_bc_oct_6c;
	ett[13] = &ett_bc_oct_6d;
	ett[14] = &ett_bc_oct_6e;
	ett[15] = &ett_bc_oct_6f;
	ett[16] = &ett_bc_oct_6g;
	ett[17] = &ett_bc_oct_7;

	last_offset = NUM_INDIVIDUAL_ELEMS;

	for (i=0; i < NUM_GSM_DTAP_MSG_MM; i++, last_offset++)
	{
		ett_gsm_dtap_msg_mm[i] = -1;
		ett[last_offset] = &ett_gsm_dtap_msg_mm[i];
	}

	for (i=0; i < NUM_GSM_DTAP_MSG_CC; i++, last_offset++)
	{
		ett_gsm_dtap_msg_cc[i] = -1;
		ett[last_offset] = &ett_gsm_dtap_msg_cc[i];
	}

	for (i=0; i < NUM_GSM_DTAP_MSG_SMS; i++, last_offset++)
	{
		ett_gsm_dtap_msg_sms[i] = -1;
		ett[last_offset] = &ett_gsm_dtap_msg_sms[i];
	}

	for (i=0; i < NUM_GSM_DTAP_MSG_SS; i++, last_offset++)
	{
		ett_gsm_dtap_msg_ss[i] = -1;
		ett[last_offset] = &ett_gsm_dtap_msg_ss[i];
	}

	for (i=0; i < NUM_GSM_DTAP_MSG_TP; i++, last_offset++)
	{
		ett_gsm_dtap_msg_tp[i] = -1;
		ett[last_offset] = &ett_gsm_dtap_msg_tp[i];
	}

	for (i=0; i < NUM_GSM_DTAP_ELEM; i++, last_offset++)
	{
		ett_gsm_dtap_elem[i] = -1;
		ett[last_offset] = &ett_gsm_dtap_elem[i];
	}

	/* Register the protocol name and description */

	proto_a_dtap =
		proto_register_protocol("GSM A-I/F DTAP", "GSM DTAP", "gsm_a_dtap");

	proto_register_field_array(proto_a_dtap, hf, array_length(hf));

	proto_register_subtree_array(ett, array_length(ett));

	/* subdissector code */
	register_dissector("gsm_a_dtap", dissect_dtap, proto_a_dtap);
}

void
proto_reg_handoff_gsm_a_dtap(void)
{
	dissector_handle_t dtap_handle;

	dtap_handle = find_dissector("gsm_a_dtap");
	dissector_add("bssap.pdu_type", BSSAP_PDU_TYPE_DTAP, dtap_handle);
	dissector_add("ranap.nas_pdu", BSSAP_PDU_TYPE_DTAP, dtap_handle);
	dissector_add("llcgprs.sapi", 1 , dtap_handle); /* GPRS Mobility Management */
	dissector_add("llcgprs.sapi", 7 , dtap_handle); /* SMS */

	data_handle = find_dissector("data");
	gsm_map_handle = find_dissector("gsm_map");
	rp_handle = find_dissector("gsm_a_rp");
}
