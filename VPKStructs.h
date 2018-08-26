//
// Created by joe on 23/08/18.
//

#ifndef FUSEVPK_VPKSTRUCTS_H
#define FUSEVPK_VPKSTRUCTS_H

#pragma pack(push, 1) // these structs should not be word aligned, but packed so structures match on-disk version

// first part of vpk file
struct VPKHeader_v2 {
  unsigned int Signature = 0x55aa1234;  // throw if any of signature or version doesn't match
  unsigned int Version = 2;

  unsigned int TreeSize;                // The size, in bytes, of the directory tree
  unsigned int FileDataSectionSize;     // How many bytes of file content are stored in this VPK file (0 in CSGO)
  unsigned int ArchiveMD5SectionSize;   // The size, in bytes, of the section containing MD5 checksums for external archive content
  unsigned int OtherMD5SectionSize;     // The size, in bytes, of the section containing MD5 checksums for content in this file (should always be 48)
  unsigned int SignatureSectionSize;    // The size, in bytes, of the section containing the public key and signature. This is either 0 (CSGO & The Ship) or 296 (HL2, HL2:DM, HL2:EP1, HL2:EP2, HL2:LC, TF2, DOD:S & CS:S)
};

// instance of a single file in the vpk, located in the tree section
struct VPKDirectoryEntry {
  unsigned int CRC;             // A 32bit CRC of the file's data.
  unsigned short PreloadBytes;  // The number of bytes directly following the entry
  unsigned short ArchiveIndex;  // if 0x7fff data follows entry, if 0 data in file area, otherwise data in _<archiveindex>.vpk
  unsigned int EntryOffset;     // offset of data from numbered vpk archive / archive data section defined in ArchiveIndex
  unsigned int EntryLength;     // If 0 entire file in preload data, else the number of bytes stored starting at EntryOffset.
  unsigned short Terminator = 0xffff;
};

// cached checksums, expect these to line up with packed file contents?
struct VPKArchiveMD5SectionEntry {
  unsigned int ArchiveIndex;    // which file was hashed (_dir.vpk, _<number>.vpk)
  unsigned int StartingOffset;  // where to start reading bytes
  unsigned int Count;           // how many bytes to check
  char MD5Checksum[16];         // expected checksum
};

// checksum of the vpk header sections
struct VPKOtherMD5Section {
  char TreeChecksum[16];                // checksum of entire tree area?
  char ArchiveMD5SectionChecksum[16];   // checksum of archive md5 hash area?
  char Unknown[16];                     // not a clue lol
};

// keyfiles for signatures
struct VPKSignatureSection {
  unsigned int PublicKeySize; // always seen as 160 (0xA0) bytes
  char *PublicKey;
  unsigned int SignatureSize; // always seen as 128 (0x80) bytes
  char *Signature;
};

#pragma pack(pop)



#endif //FUSEVPK_VPKSTRUCTS_H
