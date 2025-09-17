#include "main.h"

void configureTAR() {
  
  server.on("/maketar", HTTP_GET, [](AsyncWebServerRequest *request) {
    char path[50];
    char tarfilename[20];
  
    // get all files under de directory
    strcpy(path, "/"); // take root by default if next if = false
    if (request->hasParam("tardir")) {
      strcpy(path, request->getParam("tardir")->value().c_str());
    }
    strcpy(tarfilename, "/root"); // we'll create root.tar unless next if..
    if (strcmp(path, "/")) {
//      if (path[0] == '/') strcpy(tarfilename, path+1); // skip 1st '/'
//      else 
      strcpy(tarfilename, path);
    }
    
    if (tarfilename[strlen(tarfilename)-1] == '/') tarfilename[strlen(tarfilename)-1]=0; // get rid of last '/'
    strcat(tarfilename, ".tar");
    Serial.printf("Creating TAR file: %s\n", tarfilename);
    File tar = LittleFS.open(tarfilename, "w"); // open 'tar' for writing
    if (!tar) {
      Serial.println("Failed to open TARfile");
    }
    scan_dir(path, tar);
    for (int i=0; i<1024; i++) {
      tar.write(0);  // 2x 512 \0 to finalize the TAR
    }
    tar.close();
    Serial.printf("tarfile: %s, size %i\n", tarfilename, tar.size());
    Serial.printf("we reached this %s\n", tarfilename);
    request->send(200, "text/plain", tarfilename); 
  });   
}

void scan_dir(char * path, File tarfile) {
// comment  esp_task_wdt_deinit();
// comment  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
// comment  esp_task_wdt_add(NULL); //add current thread to WDT watch

  File dir1 = LittleFS.open(path, "r");

  time_t tarfiledate;
  struct tm tarfilecreation;
  
  char pathfilename[100];
  File nextfile = dir1.openNextFile();
  while (nextfile) {
    tarfiledate = nextfile.getLastWrite();
    localtime_r(&tarfiledate, &tarfilecreation);           // update the structure tm with the current time
    strcpy(pathfilename, path);
    strcat(pathfilename, nextfile.name());
//    strcpy(pathfilename, (path + nextfile.name()).c_str());
    if ((!nextfile.isDirectory()) && (!(String(nextfile.name()).endsWith(".tar")))) { // skip any .tar files
      Serial.printf("File:\t%s\tSize:\t%u\t%02u-%02u-%04u\n", pathfilename, nextfile.size(), tarfilecreation.tm_mday, tarfilecreation.tm_mon+1, tarfilecreation.tm_year+1900);
      create_TAR(pathfilename, tarfile, nextfile);
    }
    if (nextfile.isDirectory()) {
      strcat(pathfilename, "/");
      Serial.printf("Dir:\t%s\tSize:\t%u\t%02u-%02u-%04u\n", pathfilename, nextfile.size(), tarfilecreation.tm_mday, tarfilecreation.tm_mon+1, tarfilecreation.tm_year+1900);
      create_TAR(pathfilename, tarfile, nextfile);
      scan_dir(pathfilename, tarfile); //  Recursive
    }
    nextfile = dir1.openNextFile();
  }
// comment  esp_task_wdt_deinit();
}

void create_TAR(char * filename, File tarfile, File dir) {
  /**********************************************************************************************************************************
  * USTAR Format
  This section was original adapted from the IBM AIX manpages, and covers what I believe to be the ustar format.
  
  Tape Archive Header Block
  Every tar archive is composed of several "files". Each file has a header block describing the file, followed 
  by zero or more blocks that give the contents of the file. The end-of-archive indicator consists of two blocks 
  filled with binary zeros. Each block is a fixed size of 512 bytes.

  Blocks are grouped for physical I/O based on the "blocking factor". These groups can be written using a single 
  write(2) call. On magnetic tape, the result of this write is a single tape record 
  (sometimes called a tape block; confusing isn't it?). The last record is always a full 512 bytes. Blocks after 
  the end-of-archive zeros contain undefined data.

  The header block structure is shown in the following table. All lengths and offsets are in decimal, and all 
  lengths are in bytes. All times are in Unix standard format (seconds since epoch, UTC).

  FieldName     Offset    Length    Contents
  name          0         100       filename (no path, no slash)
  mode          100       8         file mode e.g 644 0r 755
  uid           108       8         user id
  gid           116       8         group id
  size          124       12        length of file contents
  mtime         136       12        last modification time
  cksum         148       8         file and header checksum
  typeflag      156       1         file type
  linkname      157       100       linked path name or file name
  magic         257       6         format representation for tar
  version       263       2         version representation for tar
  uname         265       32        user name
  gname         297       32        group name
  devmajor      329       8         major device representation
  devmajor      337       8         minor device representation
  prefix        345       155       path name, no trailing slashes
  remainder?    500       11        to fill the 512 block ?
  
  Names are preserved only if the characters are chosen from the POSIX portable file-name character set or if the 
  same extended character set is used between systems. During a read operation, a file can be created only if the 
  original file can be accessed using the open, stat, chdir, fcntl, or opendir subroutine.
  
  Header Block Fields
    Each field within the header block and each character on the archive medium are contiguous. There is no padding 
    between fields. More information about the specific fields and their values follows:

  name
    The file's path name is created using this field, or by using this field in connection with the prefix field. 
    If the prefix field is included, the name of the file is prefix/name. This field is null-terminated unless 
    every character is non-null. 
  mode
    Provides 9 bits for file permissions and 3 bits for SUID, SGID, and SVTX modes. All values for this field are 
    in octal. During a read operation, the designated mode bits are ignored if the user does not have equal 
    (or higher) permissions or if the modes are not supported. Numeric fields are terminated with a space and a 
    null byte. The tar.h file contains the following possible values for this field:
    
    Flag    Octal   Description
    TSUID   04000   set user ID on execution
    TSGID   02000   set group ID on execution
    TSVTX   01000   reserved
    TUREAD  00400   read by owner
    TUWRITE 00200   write by owner
    TUEXEC  00100   execute or search by owner
    TGREAD  00040   read by group
    TGWRITE 00020   write by group
    TGEXEC  00010   execute or search by group
    TOREAD  00004   read by others
    TOWRITE 00002   write by others
    TOEXEC  00001   execute or search by other
  uid
    Extracted from the corresponding archive fields unless a user with appropriate privileges restores the file. 
    In that case, the field value is extracted from the password and group files instead. Numeric fields are 
    terminated with a space and a null byte. 
  gid
    Extracted from the corresponding archive fields unless a user with appropriate privileges restores the file. 
    In that case, the field value is extracted from the password and group files instead. Numeric fields are 
    terminated with a space and a null byte. 
  size
    Value is 0 when the typeflag field is set to LNKTYPE. This field is terminated with a space only. 
  mtime
    Value is obtained from the modification-time field of the stat subroutine. This field is terminated with a space only. 
  chksum
    On calculation, the sum of all bytes in the header structure are treated as spaces. Each unsigned byte is added 
    to an unsigned integer (initialized to 0) with at least 17-bits precision. Numeric fields are terminated with 
    a space and a null byte. 
  typeflag
    The tar.h file contains the following possible values for this field:
    Flag      Value Description
    REGTYPE   '0'   regular file
    AREGTYPE  '\0'  regular file
    LNKTYPE   '1'   link
    SYMTYPE   '2'   reserved
    CHRTYPE   '3'   character special
    BLKTYPE   '4'   block special
    DIRTYPE   '5'   directory (in this case, the size field has no meaning)
    FIFOTYPE  '6'   FIFO special (archiving a FIFO file archives its existence, not contents)
    CONTTYPE  '7'   reserved

    If other values are used, the file is extracted as a regular file and a warning issued to the standard error output. 
    Numeric fields are terminated with a space and a null byte.

    The LNKTYPE flag represents a link to another file, of any type, previously archived. Such linked-to files are 
    identified by each file having the same device and file serial number. The linked-to name is specified in the 
    linkname field, including a trailing null byte.

  linkname
    Does not use the prefix field to produce a path name. If the path name or linkname value is too long, an error 
    message is returned and any action on that file or directory is canceled. This field is null-terminated 
    unless every character is non-null. 
  magic
    Contains the TMAGIC value, reflecting the extended tar archive format. In this case, the uname and gname fields 
    will contain the ASCII representation for the file owner and the file group. If a file is restored by a user 
    with the appropriate privileges, the uid and gid fields are extracted from the password and group files 
    (instead of the corresponding archive fields). This field is null-terminated. TMAGIC is equal to the 
    string "USTAR", null-terminated. 
  version
    Represents the version of the tar command used to archive the file. This field is terminated with a space only. 
  uname
    Contains the ASCII representation of the file owner. This field is null-terminated. 
  gname
    Contains the ASCII representation of the file group. This field is null-terminated. 
  devmajor
    Contains the device major number. Terminated with a space and a null byte. 
  devminor
    Contains the device minor number. Terminated with a space and a null byte. 
  prefix
    If this field is non-null, the file's path name is created using the prefix/name values together. Null-terminated 
    unless every character is non-null. 


  The checksum is calculated as the sum of all chars in the header (512 chars). The checksum itself counts as 8*32=256 (spaces)
    ***************************************************************************************************************************/

  // make the header as described above:



  char TheOwner[7], fileMode[8], dirMode[8], UID[8], s_filesize[12], filedate[12], s_checksum[8];
  char  ustar[8];
  uint32_t i;
  uint32_t checksum=256; // The checksum field itself is treated as 8*space (8*32=256)
  strcpy(TheOwner, "nobody");
  strcpy(fileMode, "0000644");
  strcpy(dirMode, "0000755");
  strcpy(UID, "0065534");
//  strcpy(UID, "0001750");
  strcpy(ustar, "ustar  ");

  for (i=0; i< strlen(filename); i++) {
    tarfile.write(filename[i]); // filename 100 characters
    checksum += (unsigned char) filename[i];
  }
  for (i = strlen(filename); i<100; i++) {
    tarfile.write(0);
  }
  for (i=0; i< 8; i++) {
    if (!dir.isDirectory()) {
      tarfile.write(fileMode[i]); // filemode 8 bytes
      checksum += (unsigned char) fileMode[i];
    }
    else {
      tarfile.write(dirMode[i]); // filemode 8 bytes
      checksum += (unsigned char) dirMode[i];
    }
  }
  for (i=0; i< 8; i++) {
    tarfile.write(UID[i]); // User ID 8 bytes
    checksum += (unsigned char) UID[i];
  }
  for (i=0; i< 8; i++) {
    tarfile.write(UID[i]); // Group ID 8 bytes
    checksum += (unsigned char) UID[i];
  }
  sprintf(s_filesize, "%011o", dir.size());
  for (i=0; i< 12; i++) {
    tarfile.write(s_filesize[i]); // File size in bytes (octal base) 12 bytes
    checksum += (unsigned char) s_filesize[i];
  }
  sprintf(filedate,  "%011o", (int) dir.getLastWrite()); 
  for (i=0; i< 12; i++) {
    tarfile.write(filedate[i]); // Last modification time in numeric Unix time format (octal) 12 bytes
    checksum += (unsigned char) filedate[i];
  }
  // need to calculate the checksum for later entries:
  for (i=0; i<8; i++) {
    checksum += (unsigned char) ustar[i];
  }
  for (i=0; i<strlen(TheOwner)+1; i++) {
    checksum += (unsigned char) TheOwner[i];
    checksum += (unsigned char) TheOwner[i];  // twice
  }
  if (!dir.isDirectory()) {
    checksum += (unsigned char) '0'; // type
  }
  else {
    checksum += (unsigned char) '5'; // type
  }
  sprintf(s_checksum, "%06o", checksum);
  s_checksum[6] = '\0';
  s_checksum[7] = ' ';
  for (i=0; i<8; i++) {
    tarfile.write(s_checksum[i]); // write checksum
  }
  if (!dir.isDirectory()) {
    tarfile.print("0"); // type = 0 (file)
  }
  else {
    tarfile.print("5"); // type = 5 (dir)
  }
  for (i=0; i< 100; i++) { // Name of linked file 100 bytes
    tarfile.write(0); 
  }
  for (i=0; i<8; i++) {
    tarfile.write(ustar[i]);// UStar indicator "ustar  " then NULL 8 bytes
  }
  tarfile.printf("%s", TheOwner); // Owner user name 32 bytes
  for (i=strlen(TheOwner); i<32; i++) {
    tarfile.write(0);         
  }
  tarfile.printf("%s", TheOwner); // Group name 32 bytes
  for (i=strlen(TheOwner); i<32; i++) {
    tarfile.write(0);         
  }
  // Device major number 8 bytes
  // Device minor number 8 bytes
  // Filename prefix 155 bytes (All 0)
  // + 12 bytes to fill block 512
  for (i=0; i<183; i++) {
    tarfile.write(0);
  }
  // then the actual data

// comment  esp_task_wdt_reset();
//  Serial.printf("Reset WDT\n");

  File syxfile = LittleFS.open(filename, "r");
  Serial.printf("Size: %i\n", dir.size());
  
  for (i=0; i< dir.size(); i++) {
    tarfile.write(syxfile.read());
//    if (!i%512) Serial.printf("%i ", i);
  }
  Serial.println();
  while (i%512) { // fill with 0's
    tarfile.write(0);
    i++;
  }
  syxfile.close();
}
