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
#include "tables.h"


/*****************************************************************************
 *                                encry_do_rdwt
 *****************************************************************************/
/**
 * Read/Write file and return byte count read/written.
 *
 * Sector map is not needed to update, since the sectors for the file have been
 * allocated and the bits are set when the file was created.
 * 
 * @return How many bytes have been read/written.
 *****************************************************************************/
//Aes algorithom

// RotWord
static inline void RotWord(unsigned char *word) {
    unsigned char temp = word[0];
    word[0] = word[1];
    word[1] = word[2];
    word[2] = word[3];
    word[3] = temp;
}

// SubWord
static inline void SubWord(unsigned char *word, int n) {
    for (int i = 0; i < n; i++)
        word[i] = S[word[i]];
}


int aes_make_enc_subkeys(const unsigned char key[16], unsigned char subKeys[11][16])
{
    if (key == 0 || subKeys == 0) {
        return 1;
    }
    int i = 0;
    unsigned char temp[4];
    
    memcpy(subKeys[0], key, 16);
   
    for (i = 1; i <= 10; i++) {
        memcpy(temp, &subKeys[i - 1][12], 4);  
        RotWord(temp);  
        // SubWord(temp,4);  
        for (int i = 0; i < 4; i++)
            temp[i] = S[temp[i]];
        
        for (int j = 0; j < 4; j++)
            subKeys[i][j] = subKeys[i - 1][j] ^ temp[j];
        
        subKeys[i][0] ^= rcon[i]; 
        
        for (int j = 4; j < 16; j += 4) {
            for (int k = 0; k < 4; k++) {
                subKeys[i][j + k] = subKeys[i - 1][j + k] ^ subKeys[i][j + k - 4];
            }
        }
    }
    return 0;
}


static inline void ShiftRows(unsigned char *m)
{
    int i;
    unsigned char m_e[16]; 
    memcpy(m_e,m,16);
	for (i=0;i<16;i++)
		m[i]=m_e[e[i]-1];
}

unsigned char mul(unsigned char a, unsigned char b) {
    unsigned char p = 0;
    unsigned char high_bit_mask = 0x80;
    unsigned char high_bit = 0;
    unsigned char modulo = 0x1B; /* x^8 + x^4 + x^3 + x + 1 */

    for (int i = 0; i < 8; i++) {
        if (b & 1)
            p ^= a;
        
        high_bit = a & high_bit_mask;
        a <<= 1;
        if (high_bit)
            a ^= modulo;
        
        b >>= 1;
    }

    return p;
}


char mul_2_table[16];
void init_mul_2_table(unsigned char *state) {
    for(int i = 0; i < 16; i++)
        mul_2_table[i] = mul(0x02, state[i]);
}

char mul_3_table[16];
void init_mul_3_table(unsigned char *state) {
    for(int i = 0; i < 16; i++)
        mul_3_table[i] = mul(0x03, state[i]);
}

void MixColumns(unsigned char *state) {
    unsigned char tmp[16];
    init_mul_2_table(state);
    init_mul_3_table(state);

    tmp[0]=(unsigned char)(mul_2_table[0] ^  mul_3_table[1] ^ state[2] ^ state[3]);
    tmp[1]=(unsigned char)(state[0] ^ mul_2_table[1] ^  mul_3_table[2] ^ state[3]);
    tmp[2]=(unsigned char)(state[0] ^ state[1] ^ mul_2_table[2] ^  mul_3_table[3]);
    tmp[3]=(unsigned char)( mul_3_table[0] ^ state[1] ^ state[2] ^ mul_2_table[3]);
    tmp[4]=(unsigned char)(mul_2_table[4] ^  mul_3_table[5] ^ state[6] ^ state[7]);
    tmp[5]=(unsigned char)(state[4] ^ mul_2_table[5] ^  mul_3_table[6] ^ state[7]);
    tmp[6]=(unsigned char)(state[4] ^ state[5] ^ mul_2_table[6] ^  mul_3_table[7]);
    tmp[7]=(unsigned char)( mul_3_table[4] ^ state[5] ^ state[6] ^ mul_2_table[7]);
    tmp[8]=(unsigned char)(mul_2_table[8] ^  mul_3_table[9] ^ state[10] ^ state[11]);
    tmp[9]=(unsigned char)(state[8] ^ mul_2_table[9] ^  mul_3_table[10] ^ state[11]);
    tmp[10]=(unsigned char)(state[8] ^ state[9] ^ mul_2_table[10] ^  mul_3_table[11]);
    tmp[11]=(unsigned char)( mul_3_table[8] ^ state[9] ^ state[10] ^ mul_2_table[11]);
    tmp[12]=(unsigned char)(mul_2_table[12] ^  mul_3_table[13] ^ state[14] ^ state[15]);
    tmp[13]=(unsigned char)(state[12] ^ mul_2_table[13] ^  mul_3_table[14] ^ state[15]);
    tmp[14]=(unsigned char)(state[12] ^ state[13] ^mul_2_table[14] ^  mul_3_table[15]);
    tmp[15]=(unsigned char)( mul_3_table[12] ^ state[13] ^ state[14] ^ mul_2_table[15]);
    memcpy(state, tmp, 16);
}


void aes_encrypt_block(const unsigned char *input, unsigned char subKeys[11][16], unsigned char *output)
{
    unsigned char AddRoundkey[16];
    //
    for(int i=0;i<16;i++)
        AddRoundkey[i]=subKeys[0][i]^input[i];
    
    //9 rounds
    for(int i=0;i<9;i++)
    {
        SubWord(AddRoundkey,16);//字节代换
        ShiftRows(AddRoundkey);//行移位
        MixColumns(AddRoundkey);//列混合

        for(int h=0;h<16;h++)
            AddRoundkey[h]=subKeys[i+1][h]^AddRoundkey[h];
    }
    //1 final rounds
    SubWord(AddRoundkey,16);//字节代换
    ShiftRows(AddRoundkey);//行移位
    for(int h=0;h<16;h++)
        output[h]=subKeys[10][h]^AddRoundkey[h];
}

char mul_e_table[16];
void init_mul_e_table(unsigned char *state) {
    for(int i = 0; i < 16; i++)
        mul_e_table[i] = mul(0x0e, state[i]);
}

char mul_b_table[16];
void init_mul_b_table(unsigned char *state) {
    for(int i = 0; i < 16; i++)
        mul_b_table[i] = mul(0x0b, state[i]);
}

char mul_d_table[16];
void init_mul_d_table(unsigned char *state) {
    for(int i = 0; i < 16; i++)
        mul_d_table[i] = mul(0x0d, state[i]);
}

char mul_9_table[16];
void init_mul_9_table(unsigned char *state) {
    for(int i = 0; i < 16; i++)
        mul_9_table[i] = mul(0x09, state[i]);
}

void InvMixColumns(unsigned char *state) {
    unsigned char tmp[16];
    init_mul_e_table(state);
    init_mul_b_table(state);
    init_mul_d_table(state);
    init_mul_9_table(state);

    tmp[0] = (unsigned char) (mul_e_table[0] ^ mul_b_table[1] ^  mul_d_table[2] ^ mul_9_table[3]);
    tmp[1] = (unsigned char) (mul_9_table[0] ^ mul_e_table[1] ^ mul_b_table[2] ^ mul_d_table[3]);
    tmp[2] = (unsigned char) (mul_d_table[0] ^ mul_9_table[1] ^ mul_e_table[2] ^ mul_b_table[3]);
    tmp[3] = (unsigned char) (mul_b_table[0] ^ mul_d_table[1] ^ mul_9_table[2] ^ mul_e_table[3]);
    tmp[4] = (unsigned char) (mul_e_table[4] ^ mul_b_table[5] ^ mul_d_table[6] ^ mul_9_table[7]);
    tmp[5] = (unsigned char) (mul_9_table[4] ^ mul_e_table[5] ^ mul_b_table[6] ^ mul_d_table[7]);
    tmp[6] = (unsigned char) (mul_d_table[4] ^ mul_9_table[5] ^ mul_e_table[6] ^ mul_b_table[7]);
    tmp[7] = (unsigned char) (mul_b_table[4] ^ mul_d_table[5] ^ mul_9_table[6] ^ mul_e_table[7]);
    tmp[8] = (unsigned char) (mul_e_table[8] ^ mul_b_table[9] ^ mul_d_table[10] ^ mul_9_table[11]);
    tmp[9] = (unsigned char) (mul_9_table[8] ^ mul_e_table[9] ^ mul_b_table[10] ^ mul_d_table[11]);
    tmp[10] = (unsigned char) (mul_d_table[8] ^ mul_9_table[9] ^ mul_e_table[10] ^ mul_b_table[11]);
    tmp[11] = (unsigned char) (mul_b_table[8] ^ mul_d_table[9] ^ mul_9_table[10] ^ mul_e_table[11]);
    tmp[12] = (unsigned char) (mul_e_table[12] ^ mul_b_table[13] ^ mul_d_table[14] ^ mul_9_table[15]);
    tmp[13] = (unsigned char) (mul_9_table[12] ^ mul_e_table[13] ^mul_b_table[14] ^ mul_d_table[15]);
    tmp[14] = (unsigned char) (mul_d_table[12] ^ mul_9_table[13] ^ mul_e_table[14] ^ mul_b_table[15]);
    tmp[15] = (unsigned char) (mul_b_table[12] ^ mul_d_table[13] ^ mul_9_table[14] ^ mul_e_table[15]);
    
    memcpy(state, tmp, 16);
}

int aes_make_dec_subkeys(const unsigned char key[16], unsigned char subKeys[11][16])
{
    if (aes_make_enc_subkeys(key, subKeys) != 0)
        return 1;
    
    for (int round = 1; round < 10; round++)
        InvMixColumns(subKeys[round]);
    
    return 0;
}

// 逆字节代换函数
static inline void InvSubWord(unsigned char *word, int n) {
    for (int i = 0; i < n; i++)
        word[i] = inv_S[word[i]];
    
}

// 逆行移位函数
static inline void InvShiftRows(unsigned char *m)
{
    int i;
    unsigned char m_e[16]; 
    memcpy(m_e,m,16);
	for (i=0;i<16;i++)
		m[i]=m_e[inv_e[i]-1];
	
}

void aes_decrypt_block(const unsigned char *input, unsigned char subKeys[11][16], unsigned char *output)
{
    unsigned char state[16];
    int i;
    // 初始轮密钥加
    for (i = 0; i < 16; i++)
        state[i] = input[i] ^ subKeys[10][i];
    
    // 执行9轮的解密操作
    for (i = 9; i > 0; i--) {
        InvSubWord(state, 16);  // 逆字节代换
        InvShiftRows(state);  // 逆行移位
        InvMixColumns(state);  // 逆列混合
        for (int j = 0; j < 16; j++)
            state[j] ^= subKeys[i][j];  // 轮密钥加    
    }

    // 最后一轮解密操作（没有列混合）
    InvSubWord(state, 16);  // 逆字节代换
    InvShiftRows(state);  // 逆行移位
    for (int j = 0; j < 16; j++)
        output[j] = subKeys[0][j]^state[j];  // 轮密钥加
}






PUBLIC int encry_do_rdwt()
{
	int fd = fs_msg.FD;	/**< file descriptor. */
	void * buf = fs_msg.BUF;/**< r/w buffer */
	int len = fs_msg.CNT;	/**< r/w bytes */

	int src = fs_msg.source;		/* caller proc nr. */

	assert((pcaller->filp[fd] >= &f_desc_table[0]) &&
	       (pcaller->filp[fd] < &f_desc_table[NR_FILE_DESC]));

	assert(NR_FILE_DESC == 64);

	//字符串密钥
	unsigned char key[] = "zstuctf";
	unsigned long key_len = sizeof(key) - 1;
	if (fs_msg.type == ENCRY_WRITE) {
		rc4_crypt(buf, len, key, key_len);
	}


	if (!(pcaller->filp[fd]->fd_mode & O_RDWR))
		return 0;

	int pos = pcaller->filp[fd]->fd_pos;

	struct inode * pin = pcaller->filp[fd]->fd_inode;

	assert(pin >= &inode_table[0] && pin < &inode_table[NR_INODE]);

	int imode = pin->i_mode & I_TYPE_MASK;

	if (imode == I_CHAR_SPECIAL) {
		int t = fs_msg.type == ENCRY_READ ? DEV_READ : DEV_WRITE;
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
		assert((fs_msg.type == ENCRY_READ) || (fs_msg.type == ENCRY_WRITE));

		int pos_end;
		int bytes_left;
		if (fs_msg.type == ENCRY_READ) {
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

			if (fs_msg.type == ENCRY_READ) {
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
		
		if (fs_msg.type == ENCRY_READ) {
			rc4_crypt(buf, len, key, key_len);
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
