/* C code produced by gperf version 3.0.4 */
/* Command-line: /usr/bin/gperf -m 10 -C -G -D -t  */
/* Computed positions: -k'1-2,6,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif


/*
 * Copyright (C) 2005 Andreas Steffen
 * Hochschule fuer Technik Rapperswil, Switzerland
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <string.h>

#include "keywords.h"

struct kw_entry {
    char *name;
    kw_token_t token;
};

#define TOTAL_KEYWORDS 182
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 17
#define MIN_HASH_VALUE 8
#define MAX_HASH_VALUE 368
/* maximum key range = 361, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (str, len)
     register const char *str;
     register unsigned int len;
{
  static const unsigned short asso_values[] =
    {
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369,  27,
      135, 369,   2, 369,   3, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369,   3, 369,  85, 369,  58,
      100,   1,  41, 100,  70,   0, 369, 157,   1,  75,
       26,  45,  34, 369,   2,  20,   2, 144,   1,   0,
        3,  14,   6, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369, 369, 369, 369, 369,
      369, 369, 369, 369, 369, 369
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
      case 4:
      case 3:
      case 2:
        hval += asso_values[(unsigned char)str[1]];
      /*FALLTHROUGH*/
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

static const struct kw_entry wordlist[] =
  {
    {"left",              KW_LEFT},
    {"right",             KW_RIGHT},
    {"lifetime",          KW_KEYLIFE},
    {"rightimei",         KW_RIGHTIMEI},
    {"leftcert",          KW_LEFTCERT},
    {"leftfirewall",      KW_LEFTFIREWALL},
    {"rightikeport",      KW_RIGHTIKEPORT},
    {"leftsendcert",      KW_LEFTSENDCERT},
    {"rightintsubnet",    KW_RIGHTINTSUBNET},
    {"leftprotoport",     KW_LEFTPROTOPORT},
    {"rightikeportnatt",  KW_RIGHTIKEPORTNATT},
    {"type",              KW_TYPE},
    {"rekey",             KW_REKEY},
    {"leftsigkey",        KW_LEFTSIGKEY},
    {"leftallowany",      KW_LEFTALLOWANY},
    {"leftcertpolicy",    KW_LEFTCERTPOLICY},
    {"rightrsasigkey",    KW_RIGHTSIGKEY},
    {"leftgroups",        KW_LEFTGROUPS},
    {"rightsubnet",       KW_RIGHTSUBNET},
    {"rightsendcert",     KW_RIGHTSENDCERT},
    {"server_nocert",     KW_SERVER_NOCERT},
    {"rightidr_apn",      KW_RIGHTIDR_APN},
    {"retrans_base",      KW_RETRANS_BASE},
    {"leftintsubnet",     KW_LEFTINTSUBNET},
    {"lifebytes",         KW_LIFEBYTES},
    {"rightsigkey",       KW_RIGHTSIGKEY},
    {"leftnexthop",       KW_LEFT_DEPRECATED},
    {"leftrsasigkey",     KW_LEFTSIGKEY},
    {"inactivity",        KW_INACTIVITY},
    {"rightprotoport",    KW_RIGHTPROTOPORT},
    {"strictcrlpolicy",   KW_STRICTCRLPOLICY},
    {"installpolicy",     KW_INSTALLPOLICY},
    {"leftdns",           KW_LEFTDNS},
    {"encrkeydisplay",    KW_ENCRKEYDISPLAY},
    {"rightfirewall",     KW_RIGHTFIREWALL},
    {"esp",               KW_ESP},
    {"rekeyfuzz",         KW_REKEYFUZZ},
    {"rightforcetsi64",   KW_RIGHTFORCETSI64},
    {"rightforcetsifull", KW_RIGHTFORCETSIFULL},
    {"retrans_tries",     KW_RETRANS_TRIES},
    {"leftforcetsi64",    KW_LEFTFORCETSI64},
    {"leftforcetsifull",  KW_LEFTFORCETSIFULL},
    {"rightsubnetwithin", KW_RIGHTSUBNET},
    {"crluri",            KW_CRLURI},
    {"plutostart",        KW_SETUP_DEPRECATED},
    {"rightsourceip",     KW_RIGHTSOURCEIP},
    {"rightcert",         KW_RIGHTCERT},
    {"leftupdown",        KW_LEFTUPDOWN},
    {"certuribase",       KW_CERTURIBASE},
    {"rightnexthop",      KW_RIGHT_DEPRECATED},
    {"rightcustcpimei",   KW_RIGHTCUSTCPIMEI},
    {"rightsourceif",     KW_RIGHTSOURCEIF},
    {"interface",         KW_INTERFACE},
    {"crlcheckinterval",  KW_SETUP_DEPRECATED},
    {"rightcustcppcscf4", KW_RIGHTCUSTCPPCSCF4},
    {"rightcustcppcscf6", KW_RIGHTCUSTCPPCSCF6},
    {"lefthostaccess",    KW_LEFTHOSTACCESS},
    {"no_initct",         KW_NO_INIT_CONTACT},
    {"fragmentation",     KW_FRAGMENTATION},
    {"retrans_to",        KW_RETRANS_TO},
    {"leftimei",          KW_LEFTIMEI},
    {"rightpcscf",        KW_RIGHTPCSCF},
    {"rightcertpolicy",   KW_RIGHTCERTPOLICY},
    {"leftsourceip",      KW_LEFTSOURCEIP},
    {"crluri1",           KW_CRLURI},
    {"interfaces",        KW_SETUP_DEPRECATED},
    {"pfs",               KW_PFS_DEPRECATED},
    {"leftsourceif",      KW_LEFTSOURCEIF},
    {"virtual_private",   KW_SETUP_DEPRECATED},
    {"mediated_by",       KW_MEDIATED_BY},
    {"tfc",               KW_TFC},
    {"force_keepalive",   KW_SETUP_DEPRECATED},
    {"righthostaccess",   KW_RIGHTHOSTACCESS},
    {"reqid",             KW_REQID},
    {"rightid",           KW_RIGHTID},
    {"leftpcscf",         KW_LEFTPCSCF},
    {"ocspuri",           KW_OCSPURI},
    {"mediation",         KW_MEDIATION},
    {"rightallowany",     KW_RIGHTALLOWANY},
    {"rekeymargin",       KW_REKEYMARGIN},
    {"lifepackets",       KW_LIFEPACKETS},
    {"forceencaps",       KW_FORCEENCAPS},
    {"xauth_identity",    KW_XAUTH_IDENTITY},
    {"eap",               KW_CONN_DEPRECATED},
    {"nat_traversal",     KW_SETUP_DEPRECATED},
    {"mobike",	           KW_MOBIKE},
    {"no_eaponly",        KW_NO_EAP_ONLY},
    {"rightdns",          KW_RIGHTDNS},
    {"compress",          KW_COMPRESS},
    {"rightgroups",       KW_RIGHTGROUPS},
    {"postpluto",         KW_SETUP_DEPRECATED},
    {"also",              KW_ALSO},
    {"packetdefault",     KW_SETUP_DEPRECATED},
    {"leftidr_apn",       KW_LEFTIDR_APN},
    {"ocspuri1",          KW_OCSPURI},
    {"ocsp",              KW_OCSP},
    {"hidetos",           KW_SETUP_DEPRECATED},
    {"fragicmp",          KW_SETUP_DEPRECATED},
    {"rightid2",          KW_RIGHTID2},
    {"leftcert2",         KW_LEFTCERT2},
    {"reauth",            KW_REAUTH},
    {"leftgroups2",       KW_LEFTGROUPS2},
    {"rightca",           KW_RIGHTCA},
    {"cacert",            KW_CACERT},
    {"ldaphost",          KW_CA_DEPRECATED},
    {"dpddelay",          KW_DPDDELAY},
    {"leftsubnet",        KW_LEFTSUBNET},
    {"leftcustcpimei",    KW_LEFTCUSTCPIMEI},
    {"ike",               KW_IKE},
    {"pfsgroup",          KW_PFS_DEPRECATED},
    {"xauth",             KW_XAUTH},
    {"leftcustcppcscf4",  KW_LEFTCUSTCPPCSCF4},
    {"leftcustcppcscf6",  KW_LEFTCUSTCPPCSCF6},
    {"rightauth",         KW_RIGHTAUTH},
    {"charonstart",       KW_SETUP_DEPRECATED},
    {"plutostderrlog",    KW_SETUP_DEPRECATED},
    {"dpdaction",         KW_DPDACTION},
    {"leftikeport",       KW_LEFTIKEPORT},
    {"rightintnetmask",   KW_RIGHTINTNETMASK},
    {"leftikeportnatt",   KW_LEFTIKEPORTNATT},
    {"oostimeout",        KW_OOSTIMEOUT},
    {"leftca",            KW_LEFTCA},
    {"nocrsend",          KW_SETUP_DEPRECATED},
    {"closeaction",       KW_CLOSEACTION},
    {"rightupdown",       KW_RIGHTUPDOWN},
    {"me_peerid",         KW_ME_PEERID},
    {"keepalivedelay",    KW_KEEPALIVEDELAY},
    {"leftsubnetwithin",  KW_LEFTSUBNET},
    {"hashandurl",        KW_HASHANDURL},
    {"mark_in",           KW_MARK_IN},
    {"ldapbase",          KW_CA_DEPRECATED},
    {"margintime",        KW_REKEYMARGIN},
    {"leftintnetmask",    KW_LEFTINTNETMASK},
    {"uniqueids",         KW_UNIQUEIDS},
    {"overridemtu",       KW_SETUP_DEPRECATED},
    {"crluri2",           KW_CRLURI2},
    {"rightca2",          KW_RIGHTCA2},
    {"dpd_noreply",       KW_DPD_NOREPLY},
    {"rightcert2",        KW_RIGHTCERT2},
    {"keylife",           KW_KEYLIFE},
    {"leftid",            KW_LEFTID},
    {"fast_reauth",       KW_FAST_REAUTH},
    {"ikelifetime",       KW_IKELIFETIME},
    {"eap_identity",      KW_EAP_IDENTITY},
    {"mark_out",          KW_MARK_OUT},
    {"aggressive",        KW_AGGRESSIVE},
    {"marginbytes",       KW_MARGINBYTES},
    {"marginpackets",     KW_MARGINPACKETS},
    {"dpdtimeout",        KW_DPDTIMEOUT},
    {"leftauth",          KW_LEFTAUTH},
    {"wsharkfiledump",    KW_WSHARKFILEDUMP},
    {"ah",                KW_AH},
    {"keyexchange",       KW_KEYEXCHANGE},
    {"leftca2",           KW_LEFTCA2},
    {"cachecrls",         KW_CACHECRLS},
    {"pkcs11module",      KW_PKCS11_DEPRECATED},
    {"rightauth2",        KW_RIGHTAUTH2},
    {"prepluto",          KW_SETUP_DEPRECATED},
    {"pkcs11keepstate",   KW_PKCS11_DEPRECATED},
    {"pkcs11proxy",       KW_PKCS11_DEPRECATED},
    {"leftid2",           KW_LEFTID2},
    {"plutodebug",        KW_SETUP_DEPRECATED},
    {"ocspuri2",          KW_OCSPURI2},
    {"rightgroups2",      KW_RIGHTGROUPS2},
    {"use_cfg_vip",       KW_USE_CFG_VIP},
    {"pkcs11initargs",    KW_PKCS11_DEPRECATED},
    {"dumpdir",           KW_SETUP_DEPRECATED},
    {"keep_alive",        KW_SETUP_DEPRECATED},
    {"ikedscp",           KW_IKEDSCP,},
    {"skipcheckcert",     KW_SKIPCHECKCERT},
    {"authby",            KW_AUTHBY},
    {"charondebug",       KW_CHARONDEBUG},
    {"wdrv_keepalive",    KW_WDRV_KEEPALIVE},
    {"modeconfig",        KW_MODECONFIG},
    {"auto",              KW_AUTO},
    {"keyingtries",       KW_KEYINGTRIES},
    {"leftauth2",         KW_LEFTAUTH2},
    {"aaa_identity",      KW_AAA_IDENTITY},
    {"mark",              KW_MARK},
    {"addrchg_reauth",    KW_ADDRCHG_REAUTH},
    {"skipcheckid",       KW_SKIPCHECKID},
    {"klipsdebug",        KW_SETUP_DEPRECATED}
  };

static const short lookup[] =
  {
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,   0,   1,
      2,   3,  -1,   4,  -1,   5,   6,   7,   8,   9,
     10,  11,  12,  -1,  -1,  -1,  13,  -1,  -1,  14,
     -1,  15,  16,  -1,  17,  18,  -1,  19,  20,  -1,
     21,  -1,  22,  23,  24,  -1,  -1,  25,  26,  27,
     28,  -1,  29,  30,  31,  32,  33,  34,  35,  36,
     37,  38,  39,  40,  41,  42,  43,  44,  -1,  45,
     -1,  46,  47,  48,  49,  50,  51,  52,  53,  54,
     55,  56,  57,  58,  59,  60,  -1,  61,  -1,  62,
     -1,  -1,  -1,  63,  64,  -1,  -1,  65,  66,  -1,
     67,  -1,  68,  69,  70,  71,  -1,  72,  73,  74,
     75,  -1,  76,  77,  78,  79,  -1,  80,  81,  82,
     -1,  -1,  -1,  83,  -1,  -1,  -1,  84,  85,  86,
     87,  -1,  88,  89,  90,  91,  92,  -1,  -1,  93,
     94,  95,  96,  97,  -1,  98,  -1,  99,  -1, 100,
    101,  -1, 102, 103,  -1,  -1, 104, 105, 106,  -1,
    107, 108, 109, 110, 111, 112, 113, 114,  -1, 115,
     -1, 116, 117,  -1, 118,  -1, 119, 120, 121,  -1,
    122, 123,  -1, 124,  -1,  -1, 125, 126, 127,  -1,
     -1,  -1, 128, 129,  -1, 130,  -1, 131,  -1, 132,
    133, 134, 135, 136, 137, 138,  -1, 139, 140, 141,
    142,  -1, 143,  -1,  -1, 144, 145, 146,  -1, 147,
     -1, 148,  -1,  -1, 149, 150,  -1, 151, 152, 153,
    154, 155, 156, 157, 158,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1, 159, 160, 161,  -1,  -1, 162, 163,
    164,  -1, 165, 166, 167,  -1, 168,  -1,  -1,  -1,
     -1,  -1, 169, 170,  -1, 171,  -1,  -1,  -1,  -1,
     -1,  -1, 172,  -1,  -1, 173,  -1,  -1, 174,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 175,
    176,  -1,  -1,  -1,  -1,  -1, 177,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
     -1, 178,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 179,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 180,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 181
  };

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct kw_entry *
in_word_set (str, len)
     register const char *str;
     register unsigned int len;
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int index = lookup[key];

          if (index >= 0)
            {
              register const char *s = wordlist[index].name;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &wordlist[index];
            }
        }
    }
  return 0;
}
