/*
 * skeleton.c
 * author: Tahseen Mohammad
 */

/* This file depends on WORDS_BIGENDIAN being defined to 1 if the host
 * processor stores words with the most significant byte first (like Motorola
 * and SPARC, unlike Intel and VAX). 
 * On little endian systems, WORDS_BIGENDIAN should be undefined.
 *
 * When using GNU Autotools, the correct value will be written into config.h
 * if the autoconf macro AC_C_BIGENDIAN is called in configure.ac.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ogg/ogg.h>

#include "skeleton.h"

#ifdef WIN32                                                                   
#define snprintf _snprintf
#endif 

#if defined(__BIG_ENDIAN__) && !defined(WORDS_BIGENDIAN)
#define WORDS_BIGENDIAN 1
#endif



static inline unsigned short
_le_16 (unsigned short s)
{
  unsigned short ret=s;
#ifdef WORDS_BIGENDIAN
  ret = (s>>8) & 0x00ffU;
  ret += (s<<8) & 0xff00U;
#endif
  return ret;
}

static inline ogg_uint32_t
_le_32 (ogg_uint32_t i)
{
   ogg_uint32_t ret=i;
#ifdef WORDS_BIGENDIAN
   ret =  (i>>24);
   ret += (i>>8) & 0x0000ff00;
   ret += (i<<8) & 0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

static inline ogg_int64_t
_le_64 (ogg_int64_t l)
{
  ogg_int64_t ret=l;
  unsigned char *ucptr = (unsigned char *)&ret;
#ifdef WORDS_BIGENDIAN
  unsigned char temp;

  temp = ucptr [0] ;
  ucptr [0] = ucptr [7] ;
  ucptr [7] = temp ;

  temp = ucptr [1] ;
  ucptr [1] = ucptr [6] ;
  ucptr [6] = temp ;

  temp = ucptr [2] ;
  ucptr [2] = ucptr [5] ;
  ucptr [5] = temp ;

  temp = ucptr [3] ;
  ucptr [3] = ucptr [4] ;
  ucptr [4] = temp ;

#endif
  return (*(ogg_int64_t *)ucptr);
}

/* write an ogg_page to a file pointer */
int write_ogg_page_to_file(ogg_page *og, FILE *out) {
   int written;
   
   written = fwrite(og->header,1, og->header_len, out);
   if (written > 0)
     written += fwrite(og->body,1, og->body_len, out);

   return written;
}

int fisbone_add_message_header_field(fisbone_packet *fp, 
                                        char *header_key, 
                                        char *header_value) {

    /* size of both key and value + ': ' + CRLF */
    int this_message_size = strlen(header_key) + strlen(header_value) + 4;
    if (fp->message_header_fields == NULL) {
        fp->message_header_fields = _ogg_calloc(this_message_size+1, sizeof(char));
    } else {
        int new_size = (fp->current_header_size + this_message_size+1) * sizeof(char);
        fp->message_header_fields = _ogg_realloc(fp->message_header_fields, new_size);
    }
    snprintf(fp->message_header_fields + fp->current_header_size, 
                this_message_size+1, 
                "%s: %s\r\n", 
                header_key, 
                header_value);
    fp->current_header_size += this_message_size;

    return 0;
}

/* create a ogg_packet from a fishead_packet structure */
int ogg_from_fishead(fishead_packet *fp,ogg_packet *op) {

    if (!fp || !op) return -1;

    memset(op, 0, sizeof(*op));
    op->packet = _ogg_calloc(FISHEAD_SIZE, sizeof(unsigned char));
    if (!op->packet) return -1;

    memset(op->packet, 0, FISHEAD_SIZE);

    memcpy (op->packet, FISHEAD_IDENTIFIER, 8); /* identifier */
    *((ogg_uint16_t*)(op->packet+8)) = _le_16 (SKELETON_VERSION_MAJOR); /* version major */
    *((ogg_uint16_t*)(op->packet+10)) = _le_16 (SKELETON_VERSION_MINOR); /* version minor */
    *((ogg_int64_t*)(op->packet+12)) = _le_64 (fp->ptime_n); /* presentationtime numerator */
    *((ogg_int64_t*)(op->packet+20)) = _le_64 (fp->ptime_d); /* presentationtime denominator */
    *((ogg_int64_t*)(op->packet+28)) = _le_64 (fp->btime_n); /* basetime numerator */
    *((ogg_int64_t*)(op->packet+36)) = _le_64 (fp->btime_d); /* basetime denominator */
    /* TODO: UTC time, set to zero for now */

    op->b_o_s = 1;   /* its the first packet of the stream */
    op->e_o_s = 0;   /* its not the last packet of the stream */
    op->bytes = FISHEAD_SIZE;  /* length of the packet in bytes */

    return 0;
}

/* create a ogg_packet from a fisbone_packet structure. 
 * call this method after the fisbone_packet is filled and all message header fields are added
 * by calling add_message_header_field method.
 */
int ogg_from_fisbone(fisbone_packet *fp,ogg_packet *op) {
    
    int packet_size;

    if (!fp || !op) return -1;

    packet_size = FISBONE_SIZE + fp->current_header_size;

    memset (op, 0, sizeof (*op));       
    op->packet = _ogg_calloc (packet_size, sizeof(unsigned char));
    if (!op->packet) return -1;

    memset (op->packet, 0, packet_size);
    memcpy (op->packet, FISBONE_IDENTIFIER, 8); /* identifier */
    *((ogg_uint32_t*)(op->packet+8)) = _le_32 (FISBONE_MESSAGE_HEADER_OFFSET); /* offset of the message header fields */
    *((ogg_uint32_t*)(op->packet+12)) = _le_32 (fp->serial_no); /* serialno of the respective stream */
    *((ogg_uint32_t*)(op->packet+16)) = _le_32 (fp->nr_header_packet); /* number of header packets */
    *((ogg_int64_t*)(op->packet+20)) = _le_64 (fp->granule_rate_n); /* granulrate numerator */
    *((ogg_int64_t*)(op->packet+28)) = _le_64 (fp->granule_rate_d); /* granulrate denominator */
    *((ogg_int64_t*)(op->packet+36)) = _le_64 (fp->start_granule); /* start granule */
    *((ogg_uint32_t*)(op->packet+44)) = _le_32 (fp->preroll); /* preroll, for theora its 0 */
    *(op->packet+48) = fp->granule_shift; /* granule shift */
    memcpy((op->packet+FISBONE_SIZE), fp->message_header_fields, fp->current_header_size);

    op->b_o_s = 0;
    op->e_o_s = 0;
    op->bytes = packet_size; /* size of the packet in bytes */

    return 0;
}

/* fills up a fishead_packet from a fishead ogg_packet of a skeleton bistream */
int fishead_from_ogg(ogg_packet *op, fishead_packet *fp)
{
	///printf("%s: op %p, fp %p\n", __func__, op, fp);
    if (!fp) return -1;

    if (memcmp(op->packet, FISHEAD_IDENTIFIER, 8))
	  return -1;
	  
	///printf("%s: ok; %i, op->packet %p, sizeof(*fp) is %i, ver is now %i\n", __func__, sizeof(ogg_uint16_t), op->packet, sizeof(*fp), (int)(fp->version_major));
	ogg_uint16_t *va = (ogg_uint16_t*)(op->packet+8);
    ogg_uint16_t *vb = (ogg_uint16_t*)(op->packet+10);
	///printf("  ... %p -> %p -- %i\n", op->packet, va, (int)(*va));
	ogg_uint16_t leva = _le_16 (*va);
	ogg_uint16_t levb = _le_16 (*vb);
	///printf("  .... %i, %i\n", (int)leva, (int)levb);
    
    unsigned short maj = *((ogg_uint16_t*)(op->packet+8)); /* version major */
    unsigned short min = *((ogg_uint16_t*)(op->packet+10)); /* version minor */

    //printf("..a\n");
    
    fp->version_major = _le_16 (maj);
    fp->version_minor = _le_16 (min);

	//printf("%s: version is %i, %i\n", __func__, fp->version_major, fp->version_minor);
    
    fp->ptime_n = _le_64 (*((ogg_int64_t*)(op->packet+12))); /* presentationtime numerator */
    fp->ptime_d = _le_64 (*((ogg_int64_t*)(op->packet+20))); /* presentationtime denominator */
    fp->btime_n = _le_64 (*((ogg_int64_t*)(op->packet+28))); /* basetime numerator */
    fp->btime_d = _le_64 (*((ogg_int64_t*)(op->packet+36))); /* basetime denominator */
    
    memcpy(fp->UTC, op->packet+44, 20);

    //printf("...done\n");
    return 0;
}

/* fills up a fisbone_packet from a fisbone ogg_packet of a skeleton bitstream */
int fisbone_from_ogg(ogg_packet *op, fisbone_packet *fp) {

    if (!fp) return -1;

    if (memcmp(op->packet, FISBONE_IDENTIFIER, 8))
	  return -1;

    fp->serial_no = _le_32 (*((ogg_uint32_t*)(op->packet+12))); /* serialno of the stream represented by this fisbone packet */
    fp->nr_header_packet = _le_32 (*((ogg_uint32_t*)(op->packet+16))); /* number of header packets */
    fp->granule_rate_n = _le_64 (*((ogg_int64_t*)(op->packet+20))); /* granulrate numerator */
    fp->granule_rate_d = _le_64 (*((ogg_int64_t*)(op->packet+28))); /* granulrate denominator */
    fp->start_granule = _le_64 (*((ogg_int64_t*)(op->packet+36))); /* start granule */
    fp->preroll = _le_32 (*((ogg_uint32_t*)(op->packet+44))); /* preroll, for theora its 0 */
    fp->granule_shift = *(op->packet+48); /* granule shift */
    fp->current_header_size = op->bytes - FISBONE_SIZE;
    fp->message_header_fields = _ogg_calloc(fp->current_header_size+1, sizeof(char));
    if (!fp->message_header_fields) return -1;
    memcpy(fp->message_header_fields, op->packet+FISBONE_SIZE, fp->current_header_size);

    return 0;
}

int add_fishead_to_stream(ogg_stream_state *os, fishead_packet *fp) {

    ogg_packet op;
    int ret;

    ret = ogg_from_fishead(fp, &op);
    if (ret<0) return ret;
    ogg_stream_packetin(os, &op);
    _ogg_free(op.packet);

    return 0;
}

int add_fisbone_to_stream(ogg_stream_state *os, fisbone_packet *fp) {

    ogg_packet op;
    int ret;

    ret = ogg_from_fisbone(fp, &op);
    if (ret<0) return ret;
    ogg_stream_packetin(os, &op);
    _ogg_free(op.packet);

    return 0;
}

int add_eos_packet_to_stream(ogg_stream_state *os) {

    ogg_packet op;

    memset (&op, 0, sizeof(op));
    op.e_o_s = 1;
    ogg_stream_packetin(os, &op);

    return 0;
}

int flush_ogg_stream_to_file(ogg_stream_state *os, FILE *out) {

    ogg_page og;
    int result;

    while((result = ogg_stream_flush(os, &og)))
    {
        if(!result) break;
        result = write_ogg_page_to_file(&og, out);
        if(result != og.header_len + og.body_len)
            return 1;
    }

    return 0;
}
