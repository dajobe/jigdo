/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2003  |  richard@
  | \/�|  Richard Atterer          |  atterer.net
  � '` �
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
  const char* str = ".�<<�V��GC�&m2��'��>��U+����EN�w�k�y�t�7>��@v��;))k�?M�9���2dI�5HN��^�6�(i|�e&5_����iH��KW-\t��.����e�5(\tdqB{fQ��h�%��DF���D`(`�^��F�|�j~k���}-x@P.��m3��exup��8�w$�O{\tY)u           �L�H����V�$����dn^*���,ų�':y��ծk�.�~2٪Pg,ʧХhQk8d�9�`C���|�n�5���nU�Q�Թh�yo}�s[qh�NJJ�\"�F7�u$��@��ByYT�G\\11f]}kuMH)���7{�v��[�scX�E��gr_�ɨ���G�T����ND0L��n��o�/Ϡ�&\"/I�@&�y�Y��u/�l�lA��Kf��*6g\tϩ_Z+�^�\\�n��ePF�Mp�<#��Y�u=[�;C�ͣ@(-*!@���1��H�k�w�b�>$��a1�F�����Z\tV`ŨKbI!��}�1}�z���Vv�w�\t��2V�F~��F��-ǩw%�)� i��dI���un�f��(%��_d\t$O��t�/�'��J�Mg�\"mv~\t�g�R@�Lh�'�<FKɻJ!/�Mmf��׮'�3E�cP6�'F3�Y*�.�l˦o'�O����8\t�i�6�V�L�EZ��[%��*�c;U�I<�}�߬i@j�2���X�C}�s�\t�EVz�\t��.��|[?E���8k~M�%�s�a��g2�h6�P8�r\"�ͱ�u)��ɮ�q�,7sʸH�X���,���=����bf�6���:D�&��$O\tw������ͭ�**\\������Ihy���`�!�K�㩵5|�tt3��`@ZCN�q�*٭�� ��+4EeY�Y��e'&�w\t�a�V�R�2oaI�p)��G�w�^|dn�Y7���#�lA���D���=\\6�ح'�tT�n�17��ɠMg���X9��}�M��d�Qȵ�$�Y,��spYo���7l�!�*�0y�Q�`��]\\�~a��,��-�%�#�9fU|�Ȳ����9�S�KŽO���BΧ _��$J�*?kT�u-�'�[��5�aj��{���LNԣ��4#Y�F��T����,�!�m����AA-l�2HZN)�Z�­5EM*                                     &2WtY}��A,��f�`��m�BDj�[�Wⷾ:0p�v�x|��@zӿJf��9o>���(��pU�}��,_^�f\\{h�K�?�=�<Zۡ��\\����-��?4��aF5andQ�z��}�ZQ����Uո�N����g��o$d�#~�vuǰ���E��B#^Z�B�i��=��oxe�!�qQ��wP ��dTlNql�עV#TC*�.@��;��5f�1�B�I�SrX���GK�gТ�'8�v��B��Y��9�&ChE'#�yC�/�������Q<�0�Pm��� ����oV��iG~�S\t\t��(��s�b�82N�w�q�u6@��T�X!71:UKs�4oT��NVP� 4Z��P��-t��lA�b�C��p����'�0�?�XGp,>zlK����zT&�g)e�!��x���)�78MA�w�&��\"��y�ph�z4��F��F_1                                                       �3��Ϸ?�Q2&�bIC:L�st�v�\t(�iX��jx�r�Rq�.�c�`� �J)�L��j�@9�h񡡭w�:;n34��1YQѾ�4\t;��P=�C5��$,�!4!/Zd<'��@����K�s��/�$\t      CNJ� �#�RU��T�'�v1Ԧl����>t��*�����\t���&�p=��a����m��<��N �-S_�Z6��F�A,u�k�0`W:�߱�]G�<��b`Zd�/��q�D@n�[?yr����jU�P���>pAG��^���h���/��sX�/�9�b>F���9�`�o�}BQ ��                                                                               ̼P_��覶%R3�l~L��*C�q�     c��:a��&�k-��{!>�֮?���X���zʾ]�a<�P�y�7�>w���+>KIuX�;,@\\Ou�U�s�3��I���+��=t_�\\���e1<�?�WH�P���P���QR|H,[�.^}�P�Ux��AW�M�7�7�ꡡ2|!K�\"�#�\"�f�X�p9�B�LTDP_j,�ծ��tt-ͭ� u������8s-��ݠ|y����j|���0t�̵��O�m?��BL�d�/����f>�٫�1";
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
