/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  A 32 or 64 bit rolling checksum

  Command line argument: Name of file to use for test. Will not output
  anything if test is OK.

  #test-deps util/rsyncsum.o

*/

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>

#include <bstream.hh>
#include <rsyncsum.hh>
#include <log.hh>
//______________________________________________________________________

#ifdef CREATE_CONSTANTS

int main(int, char* argv[]) {
  uint32 data[256];
  FILE* f = fopen(argv[1], "r");
  fread(data, sizeof(uint32), 256, f);
  for (int row = 0; row < 64; ++row) {
    uint32* r = data + 4*row;
    printf("  0x%08x, 0x%08x, 0x%08x, 0x%08x,\n", r[0], r[1], r[2], r[3]);
  }
}

//======================================================================

#else

namespace {
  int errs;
  char estr[17] = "                ";
}

inline uint32 get_checksum1(const byte *buf1,int len) {
  RsyncSum s(buf1, len);
  return s.get();
}

inline void error(int i, bool assertion) {
  if (assertion) {
    if (estr[i] == ' ')
      estr[i] = '.';
  } else {
    ++errs;
    estr[i] = '*';
  }
}

void printBlockSums(size_t blockSize, const char* fileName) {
  bifstream file(fileName, ios::binary);
  byte buf[blockSize];
  byte* bufEnd = buf + blockSize;

  while (file) {
    // read another block
    byte* cur = buf;
    while (cur < bufEnd && file) {
      readBytes(file, cur, bufEnd - cur); // Fill buffer
      cur += file.gcount();
    }
    RsyncSum64 sum(buf, cur - buf);
    cout << ' ' << sum
         << " (lo=0x" << hex << sum.getLo()
         << ", hi=0x" << hex << sum.getHi() << ')' << endl;
  }
  exit(0);
}

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);

  if (argc == 3) {
    // 2 cmdline args, blocksize and filename. Print RsyncSums of all blocks
    printBlockSums(atoi(argv[1]), argv[2]);
  }
//  else if (argc != 2) {
//     cerr << "Try " << argv[0] << " [blocksize] filename" << endl;
//     exit(1);
//   }

#if 0
  // 1 cmdline arg => play around a little with the file data
  const int CHUNK = 8192;
  FILE* f = fopen(argv[1], "r");
  byte* mem = (byte*)malloc(CHUNK);
  size_t size = CHUNK; /* of *mem */
  size_t read = 0;
  size_t totalread = 0; /* bytes in file */

  while ((read = fread(mem + totalread, 1, CHUNK, f)) != 0 && !feof(f)) {
    totalread += read;
    size = totalread + CHUNK;
    /*printf("%d  %d  %d\n", read, totalread, size);*/
    mem = (byte*)realloc(mem, size);
  }
  totalread += read;
#else
  const char* str = ".Ú<<³VÚÌGCÉ&m2¬¡'·Ø>öêU+ÆÊÎôENèwÌkºy¾tü7>¸Ä@vÿĞ;))k¦?M¶9±¨Ì2dIÎ5HNÜı^º6´(i|Ìe&5_ëâ÷­iHë§àKW-\tÊ÷.²ÒäÄeø5(\tdqB{fQ¿¼h¢%ÌÃDF³ûÑD`(`ã^ÌèFñ£|§j~k¼Şë}-x@P.Ïòm3Äïexupç»è8¾w$óO{\tY)u           ²L£H¼£îÍV½$¶õªÙdn^*³¤ü,Å³Ù':y½ïÕ®k».ú~2ÙªPg,Ê§Ğ¥hQk8dè9¤`CÕĞà¶|¢nê5èÅñ°­nUÆQâÔ¹h¡yo}ãs[qhÒNJJç\"ÂF7Ãu$ßô@ûòByYT£G\\11f]}kuMH)ë»ï¸Ü7{ğvÓé[¾scX£E³Ágr_ÑÉ¨ÍóÚGÊT·µ§íND0LĞÔnàôoÅ/Ï ü&\"/Iá@&äy¦Yé¹ïu/¤l¯lAÄàKf¥¨*6g\tÏ©_Z+û^ı\\ãnàÃePFôMp¤<#ØÂYÁu=[ú;CğÍ£@(-*!@Çì½á1±¿H±k¿wöbÜ>$íÌa1ÚF½³¾¨°Z\tV`Å¨KbI!¦Í}¦1}ÛzôÌãVvÁwÀ\tÜÑ2VèF~¬ÃFÈÖ-Ç©w%«)¸ iÁ³dIÒü½unºf ·(%¾ı_d\t$OÃÚtÂ/Á'§ªJ¬Mgı\"mv~\tégµR@¡LhÃ'Ä<FKÉ»J!/ÒMmf»£×®'ù3E¨cP6¹'F3²Y*ß.ÆlË¦o'ôOçèÚæ8\táiè6ùV±LãEZÉÛ[%úü*İc;U¯I<î}åß¬i@j²2ÌÌáXìµC}Ñs¬\tâEVzõ\tí×.ÏÅ|[?E·Àè8k~Mò%£s°aãÊg2úh6âP8°r\"ÙÍ±çu)İÁÉ®õqÍ,7sÊ¸HáXÏÃÃ,ÕØí=£ÓÁábfÿ6¾ŞÊ:Dó&õ¥$O\tw¿®­æÉÇÍ­Ò**\\äèîÉëñIhy¤Õê`­!ìKÈã©µ5|ått3Êë`@ZCN°qí*Ù­íğ à½ò+4EeYÂYÕôe'&½w\tÈaäVÕRç2oaI p)èÌG÷wå^|dnïY7µúì#÷lAşÉİD¯üÑ=\\6ÅØ­'ĞtTÿnµ17ªÈÉ Mg¯êıX9·ş}§MìèdîQÈµ¢$ùY,ş¹spYoı¹¯7lİ!¸*©0yíQÆ`Øõ]\\Û~a«ä,©©-Ä%ü#¸9fU|¹È²ØòÇû9àSıKÅ½O»¦àBÎ§ _¤å$J«*?kTİu- 'ü[ÃÇ5½ajğºû{­ïşLNÔ£¢ê4#YÄFû¥T ¢Ïé,æ!ÜmäíìùAA-l¦2HZN)ÓZåÂ­5EM*                                     &2WtY}÷êA,Ïğ¤ºf¿`âØmïBDj¨[¤Wâ·¾:0pÉváx|âÁ@zÓ¿Jfğú9o>äÕÒ(ÛğpUÇ}úì,_^Çf\\{hç·K ?¢=Õ<ZÛ¡±Ê\\Òêêê-Øò?4ºÛaF5andQæzëĞ}îZQÎöÍùUÕ¸³Nçìå¯Üg¡¼o$d¶#~şvuÇ°ùÌáEÌÀB#^Zò¬Bæiû®=ËÄoxeÏ!¹qQµ¼wP ½ßdTlNqlÂ×¢V#TC*¨.@¾©;ÇÒ5fã1ùBçIÒSrX½ÛêGK¯gĞ¢¨'8ÚvååBûªYºğ9¶&ChE'#éyCµ/ õòõÿóíQ<¯0±Pm«øÙ  ú¼ì¼oVûäiG~¯S\t\tÉñ(÷Ös·b¶82Núwªq³u6@ö«TµX!71:UKsÈ4oTÉëNVPÖ 4Z­¬PñÑ-t»ªlAÇb²Cóâp«¯ËĞ'Á0é?öXGp,>zlKáìîÆzT&üg)eí!íøxëóÓ)é78MA«wÇ&Õäº\"çá¶yıph¦z4À´FûíF_1                                                       ï3·ÒÏ·?ÛQ2&ËbIC:LØst®v®\t(öiX£ëjxõr¼Rq».¸cü`² ¿J)úL¨ıjÀ@9Èhñ¡¡­wï:;n34çÚ1YQÑ¾Ş4\t;¥·P=æC5Íâ$,Ø!4!/Zd<'åÚ@£¡ñÿK¬sÖÙ/¾$\t      CNJÔ ®#æRU®´TË'å¢v1Ô¦lÑÚöÃ>t©à*ãÓÁ³¹\t­£Ï&ßp=ÿÇaÂúÚÅmîæ«<Ï÷N ä-S_ØZ6æáFæA,uÓkì0`W:Şß±¤]Gä<ğ÷b`Zd·/Ôàqîº¦¤D@n¿[?yr§¨ĞíjUæPé´ïí>pAG¶Á^ÀÕÒhåÛÍ/¡¤sXÕ/ø9ïb>Fíßá9¿`«o®}BQ ¾â                                                                               Ì¼P_ÁÑè¦¶%R3çl~L®ã¡*CñqÚ     cÃü:aÒã&Ğk-Áú{!>ªÖ®?ëÚñX©ÔÇzÊ¾]³a<ÔP¾y¸7Ç>w¼°Â+>KIuX¥;,@\\OuºUøs£3óòIâäß+ ´=t_È\\¤şàe1<ï?ÆWH¦P¥ÛğPªµİQR|H,[ğ¯.^}öP¨Ux¨ÊAWÀMá7è7èê¡¡2|!K½\"Ä#¾\"ıf·Xøp9¹BÿLTDP_j,ò¸Õ®şõtt-Í­è u¾ş¿­©²8s-ğİİ |yçÄÉÚj|à©şı0t¿ÌµÔÀOÿm?ı¨BLÒdä/ëèÔıf>§Ù«Ö1";
  const byte* mem = reinterpret_cast<const byte*>(str);
  size_t totalread = strlen(str);
#endif
  //________________________________________

  errs = 0;

  {
    RsyncSum rs;
    rs.addBack(mem, totalread);
    error(0, get_checksum1(mem, totalread) == rs.get());

    if (totalread > 256) {
      RsyncSum roll(mem + 32, 64);
      RsyncSum noRoll(roll);
      rs.reset().addBack(mem, 128);
      for (int i = 0; i < 32; ++i) {
        RsyncSum x = rs;
        // add stuff to end
        x.addBack(mem + 128, i);
        error(1, get_checksum1(mem, 128 + i) == x.get());
        // roll by removing one byte at front, adding one at end
        roll.removeFront(mem[32+i], 64).addBack(mem[32+64+i]);
      }
      error(2, get_checksum1(mem+64, 64) == roll.get());
      // roll by 32 bytes in one go
      noRoll.removeFront(mem+32, 32, 64).addBack(mem+32+64, 32);
      error(3, roll == noRoll);
    }
  }
  //____________________
  {
    RsyncSum64 rs, y;

    if (totalread > 256) {
      RsyncSum64 roll(mem + 32, 64);
      RsyncSum64 noRoll(roll);
      rs.reset().addBack(mem, 128);
      for (int i = 0; i < 32; ++i) {
        RsyncSum64 x = rs;
        // add stuff to end
        x.addBack(mem + 128, i);
        error(4, y.reset().addBack(mem, 128 + i) == x);
        // roll by removing one byte at front, adding one at end
        roll.removeFront(mem[32+i], 64).addBack(mem[32+64+i]);
      }
      error(2, y.reset().addBack(mem+64, 64) == roll);
      // roll by 32 bytes in one go
      noRoll.removeFront(mem+32, 32, 64).addBack(mem+32+64, 32);
      error(3, roll == noRoll);
    }
  }

  if (errs != 0) {
    printf("%s %s\n", estr, argv[1]);
    return 1;
  }
  //________________________________________

  return 0;
}

#endif
