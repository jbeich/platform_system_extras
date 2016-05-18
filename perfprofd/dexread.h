static constexpr uint32_t EndianConstant = 0x12345678;
static constexpr uint32_t ReverseEndianConst = 0x78563412;
static constexpr unsigned char dexMagic = {0x64, 0x65, 0x78, 0x0a};
static constexpr unsigned char dexVersion35 = {0x30, 0x33, 0x35, 0x00};
static constexpr unsigned char dexVersion37 = {0x30, 0x33, 0x37, 0x00};

// https://source.android.com/devices/tech/dalvik/dex-format.html#header-item

typedef struct {
  unsigned char[4] magic;
  unsigned char[4] version;
  uint32      checksum_t;
  unsigned char[20] sha1sig ;
  uint32      filesize_t;
  uint32    headersize_t;
  uint32     endiantag_t;
  uint32      linksize_t;
  uint32       linkoff_t;
  uint32        mapoff_t;
  uint32 stringidssize_t;
  uint32  stringidsoff_t;
  uint32   typeidssize_t;
  uint32    typeidsoff_t;
  uint32  protoidssize_t;
  uint32   protoidsoff_t;
  uint32  pieldidssize_t;
  uint32   fieldidsoff_t;
  uint32 methodidssize_t;
  uint32  methodidsoff_t;
  uint32 classdefssize_t;
  uint32  classdefsoff_t;
  uint32      datasize_t;
  uint32       dataoff_t;
} DexFileHeader;
