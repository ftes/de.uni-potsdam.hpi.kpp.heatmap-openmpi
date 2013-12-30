#ifndef MY_BASE64
#define MY_BASE64

std::string base64_encode(unsigned char* bytes_to_encode, unsigned int in_len);
std::string base64_encode_file(std::string fileName);

#endif // MY_BASE64
