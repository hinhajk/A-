/*************************************************************************//**
 *****************************************************************************
 * @file   read_write.c
 * @brief  
 * @author Forrest Y. Yu
 * @date   2008
 *****************************************************************************
 *****************************************************************************/

#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"
#include "aes.h"
#include "rand.h"

#define AES_KEY_SIZE 256
#define AES_BLOCK_SIZE 256

/*****************************************************************************
 *                                do_rdwt
 *****************************************************************************/
/**
 * Read/Write file and return byte count read/written.
 *
 * Sector map is not needed to update, since the sectors for the file have been
 * allocated and the bits are set when the file was created.
 * 
 * @return How many bytes have been read/written.
 *****************************************************************************/
//AES Encryption/Decryption
static AES_KEY aes_key;

//initial of AES key
void init_aes_key(const unsigned char *key){
	AES_set_encrypt_key(key, AES_KEY_SIZE, &aes_key);
	AES_set_decrypt_key(key, AES_KEY_SIZE, &aes_key);
}

//AES Encryption
void aes_encrypt(const unsigned char *in, unsigned char *out, const AES_KEY *key){
	AES_cbc_encrypt(in, out, AES_BLOCK_SIZE, key, NULL, AES_ENCRYPT);
}

//AES Decryption
void aes_decrypt(const unsigned char *in, unsigned char *out, const AES_KEY *key){
	AES_cbc_encrypt(in, out, AES_BLOCK_SIZE, key, NULL, AES_DECRYPT);
}

//pad to AES block
void pad_to_aes_block_size(unsigned char *buf, int len, int block_size) {
	int padding_len = block_size - (len % block_size);
	memset(buf + len, padding_len, padding_len);
}


//removal of AES padding
void unpad_from_aes_block_size(unsigned char *buf, int len, int block_size) {
	int padding_len = buf[len - 1];
	len -= padding_len;
	//ensure true padding
	for(int i = len; i < len + padding_len; i++) {
		if(buf[i] != padding_len) {
			//there is a mistake and exit simply
			printf("Padding verification failed!\n");
			exit(1);
		}
	}
}

PUBLIC int original_do_rdwt()
{
	int fd = fs_msg.FD;	/**< file descriptor. */
	void * buf = fs_msg.BUF;/**< r/w buffer */
	int len = fs_msg.CNT;	/**< r/w bytes */

	int src = fs_msg.source;		/* caller proc nr. */

	assert((pcaller->filp[fd] >= &f_desc_table[0]) &&
	       (pcaller->filp[fd] < &f_desc_table[NR_FILE_DESC]));

	if (!(pcaller->filp[fd]->fd_mode & O_RDWR))
		return 0;

	int pos = pcaller->filp[fd]->fd_pos;

	struct inode * pin = pcaller->filp[fd]->fd_inode;

	assert(pin >= &inode_table[0] && pin < &inode_table[NR_INODE]);

	int imode = pin->i_mode & I_TYPE_MASK;

	if (imode == I_CHAR_SPECIAL) {
		int t = fs_msg.type == READ ? DEV_READ : DEV_WRITE;
		fs_msg.type = t;

		int dev = pin->i_start_sect;
		assert(MAJOR(dev) == 4);

		fs_msg.DEVICE	= MINOR(dev);
		fs_msg.BUF	= buf;
		fs_msg.CNT	= len;
		fs_msg.PROC_NR	= src;
		assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
		send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &fs_msg);
		assert(fs_msg.CNT == len);

		return fs_msg.CNT;
	}
	else {
		assert(pin->i_mode == I_REGULAR || pin->i_mode == I_DIRECTORY);
		assert((fs_msg.type == READ) || (fs_msg.type == WRITE));

		int pos_end;
		int bytes_left;
		if (fs_msg.type == READ) {
			pos_end = min(pos + len, pin->i_size);
			bytes_left = min(len, pin->i_size - pos);
		}
		else {		/* WRITE */
			pos_end = min(pos + len, pin->i_nr_sects * SECTOR_SIZE);
			bytes_left = len;
		}

		int off = pos % SECTOR_SIZE;
		int rw_sect_min=pin->i_start_sect+(pos>>SECTOR_SIZE_SHIFT);
		int rw_sect_max=pin->i_start_sect+(pos_end>>SECTOR_SIZE_SHIFT);

		int chunk = min(rw_sect_max - rw_sect_min + 1,
				FSBUF_SIZE >> SECTOR_SIZE_SHIFT);

		int bytes_rw = 0;
		int i;
		for (i = rw_sect_min; i <= rw_sect_max; i += chunk) {
			/* read/write this amount of bytes every time */
			int bytes = min(bytes_left, chunk * SECTOR_SIZE - off);
			rw_sector(DEV_READ,
				  pin->i_dev,
				  i * SECTOR_SIZE,
				  chunk * SECTOR_SIZE,
				  TASK_FS,
				  fsbuf);

			if (fs_msg.type == READ) {
				phys_copy((void*)va2la(src, buf + bytes_rw),
					  (void*)va2la(TASK_FS, fsbuf + off),
					  bytes);
			}
			else {	/* WRITE */
				phys_copy((void*)va2la(TASK_FS, fsbuf + off),
					  (void*)va2la(src, buf + bytes_rw),
					  bytes);
				rw_sector(DEV_WRITE,
					  pin->i_dev,
					  i * SECTOR_SIZE,
					  chunk * SECTOR_SIZE,
					  TASK_FS,
					  fsbuf);
			}
			off = 0;
			bytes_rw += bytes;
			pcaller->filp[fd]->fd_pos += bytes;
			bytes_left -= bytes;
		}

		if (pcaller->filp[fd]->fd_pos > pin->i_size) {
			/* update inode::size */
			pin->i_size = pcaller->filp[fd]->fd_pos;
			/* write the updated i-node back to disk */
			sync_inode(pin);
		}

		return bytes_rw;
	}
}

PUBLIC int do_rdwt()
{
	int fd = fs_msg.FD;	/**< file descriptor. */
	void * buf = fs_msg.BUF;/**< r/w buffer */
	int len = fs_msg.CNT;	/**< r/w bytes */

	int src = fs_msg.source;		/* caller proc nr. */

	assert((pcaller->filp[fd] >= &f_desc_table[0]) &&
	       (pcaller->filp[fd] < &f_desc_table[NR_FILE_DESC]));

	if (!(pcaller->filp[fd]->fd_mode & O_RDWR))
		return 0;

	int pos = pcaller->filp[fd]->fd_pos;

	struct inode * pin = pcaller->filp[fd]->fd_inode;

	assert(pin >= &inode_table[0] && pin < &inode_table[NR_INODE]);

	int imode = pin->i_mode & I_TYPE_MASK;

	static unsigned char aes_key_data[AES_KEY_SIZE / 8];//store of key

	static int key_initialized = 0;
	
	if(!key_initialized) {
	//generate key randomly
		if(!RAND_bytes(aes_key_data, sizeof(aes_key_data))) {
			printf("Failed to generate AES key!\n");
			exit(1);
		}
		init_aes_key(aes_key_data);
		key_initialized = 1;
	}

	if (imode == I_CHAR_SPECIAL) {
		int t = fs_msg.type == READ ? DEV_READ : DEV_WRITE;
		fs_msg.type = t;

		int dev = pin->i_start_sect;
		assert(MAJOR(dev) == 4);

		fs_msg.DEVICE	= MINOR(dev);
		fs_msg.BUF	= buf;
		fs_msg.CNT	= len;
		fs_msg.PROC_NR	= src;
		assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
		send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &fs_msg);
		assert(fs_msg.CNT == len);

		return fs_msg.CNT;
	}
	else {
		assert(pin->i_mode == I_REGULAR || pin->i_mode == I_DIRECTORY);
		assert((fs_msg.type == READ) || (fs_msg.type == WRITE));

		unsigned char *temp_buf = malloc(len + AES_BLOCK_SIZE);
		if(!temp_buf) {
			printf("Failed to allocate memory for AES buffer!\n");
			return 0;
		}

		int pos_end;
		int bytes_left;
		if (fs_msg.type == READ) {
			pos_end = min(pos + len, pin->i_size);
			bytes_left = min(len, pin->i_size - pos);

			//read data to the tempary buffer and then decrypt it
			int bytes_read = original_do_rdwt(fd, temp_buf + AES_BLOCK_SIZE, bytes_left);
			unpad_from_aes_block_sized(temp_buf + AES_BLOCK_SIZE, bytes_read, AES_BLOCK_SIZE);
			aes_decrypt(temp_buf + AES_BLOCK_SIZE, temp_buf, &aes_key);
			memcpy(buf, temp_buf, bytes_read);
		}
		else {		/* WRITE */
			pos_end = min(pos + len, pin->i_nr_sects * SECTOR_SIZE);
			bytes_left = len;

			//encrypt data and then write it to file
			pad_to_aes_block_size((unsigned char *)buf, len, AES_BLOCK_SIZE);
			aes_encrypt((unsigned char *)buf, temp_buf, &aes_key);
			int bytes_written = original_do_rdwt(fd, temp_buf, len + AES_BLOCK_SIZE - (len % AES_BLOCK_SIZE == 0 ? 0 : AES_BLOCK_SIZE - (len % AES_BLOCK_SIZE)));

			//refresh the location and size of file
			pcaller->filp[fd]->fd_pos += bytes_written - (AES_BLOCK_SIZE - (len % AES_BLOCK_SIZE == 0 ? 0 : AES_BLOCK_SIZE - (len - len % AES_BLOCK_SIZE)));
			if(pcaller->filp[fd]->fd_pos > pin->i_size) {
				pin->i_size = pcaller->filp[fd]->fd_pos;
				sync_inode(pin);
			}

		}

		free(temp_buf);
		return fs_msg.type == READ ? bytes_left : len;
	}
	
	return 0;
}

		
