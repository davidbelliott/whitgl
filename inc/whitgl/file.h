#ifndef WHITGL_FILE_H_
#define WHITGL_FILE_H_

bool whitgl_file_save(const char* fileName, int size, const void* data);
bool whitgl_file_load(const char* fileName, int size, void* data);
bool whitgl_file_delete(const char* fileName);

bool whitgl_file_save_z(const char* fileName, int size, const void* data);
bool whitgl_file_load_z(const char* fileName, int size, void* data);

#endif // WHITGL_FILE_H_
