#include "stdio.h"
#include "windows.h"
#include "pspgw.h"
#include "rs232.h"

#define PSPCOMPORT	11
#define BATCOMPORT	12

#define PSPPORT	(PSPCOMPORT-1)
#define BATPORT	(BATCOMPORT-1)
#define BUFFSIZE (4096)
unsigned char battbuff[BUFFSIZE];
unsigned char pspbuff[BUFFSIZE];

char mybattserialmsg[]={0xA5, 0x06, 0x06, 0x86, 0x3D, 0x00, 0x8A, 0x01};
char pandoraserialmsg[]={0xA5, 0x06, 0x06, 0xFF, 0xFF, 0xFF, 0xFF, 0x52};


int getbatt_byte( unsigned char *buff ){
		return( RS232_PollComport( BATPORT, buff, 1 ));		
}	

int getpsp_byte( unsigned char *buff ){
		return( RS232_PollComport( PSPPORT, buff, 1 ));	
}		


typedef int (*sendfn_t)(unsigned char *buffer, int size );
//fault_handler_ack_t (*error_sorter)(chademo_faults_t);	


typedef struct procstate_s{
	unsigned char discardsrc;
	int bufferedchars;
	unsigned char  *buffer;
	int buffsize;
	sendfn_t sendfn;
}procstate_t;
	

int comparearrays( unsigned char *in1, unsigned char *in2 , int length ){
	while (length-- > 0){
		if (*(in1++) != *(in2++)){
			return 1;
		}
	}
	return 0;
}

void forward_message( procstate_t *state ) {
	int i=0;
	int chk=0;
	if( state->bufferedchars > 0 ){
		if( state->buffer[0] == 0x5A || state->buffer[0] == 0xA5 ){
			if( state->bufferedchars > 1 ){
				if( state->buffer[1] >=2 ){
					if( state->bufferedchars >= (state->buffer[1]+2) ){	// Full message received
						if( state->discardsrc == state->buffer[0]){
							state->bufferedchars = 0;// Discard received msg
							printf( "Message discarded from 0x%02x source\n", state->discardsrc);
						}
						// Check, print and forward received msg
						for( i=0, chk=0;i < state->buffer[1] + 2; i++ ){
							printf("0x%02x, ",state->buffer[i] );
							chk+=state->buffer[i];
						}			
						if( (chk &= 0xFF)!= 0xFF ){
							printf("Checksum error!!! chk: 0x%02x\n",chk);
						}	
						printf("\n");
						
						if( !comparearrays( state->buffer, mybattserialmsg , sizeof(mybattserialmsg) )){
							state->sendfn( pandoraserialmsg, sizeof(pandoraserialmsg) );
							state->bufferedchars = 0;
							return;
						}
					} else {
						return;
					}
				} else {
					return;
				}
			} else{
				return;	//Wait for more characters...
			}
		}
	}
		state->sendfn( state->buffer, state->bufferedchars );
		state->bufferedchars = 0;
}


int process_message( unsigned char newchar, procstate_t *state ) {
	if( state->bufferedchars + 1 <= state->buffsize ){
		state->buffer[state->bufferedchars++] = newchar;
	}
	else{
		printf( "Buffer overflow\n" );
		exit(-1);
	}
	
	forward_message( state );
}

int sendbatt_buf( unsigned char *buf, int size ){
	return RS232_SendBuf( BATPORT, buf, size);
}

int sendpsp_buf( unsigned char *buf, int size ){
	return RS232_SendBuf( PSPPORT, buf, size);
}
		
procstate_t	battmsgstate={
	.discardsrc = 0x5A,
	.bufferedchars = 0,
	.buffer = battbuff,
	.buffsize = sizeof(battbuff),
	.sendfn = sendpsp_buf
};	

procstate_t	pspmsgstate={
	.discardsrc = 0xA5,
	.bufferedchars = 0,
	.buffer = pspbuff,
	.buffsize = sizeof(pspbuff),
	.sendfn = sendbatt_buf
};
	
	
int main( void ){

	if ( RS232_OpenComport( BATPORT, 19200, "8E1") ){
		printf("Error opening Battery COM port. Exiting...\n");
		exit -1;
	}
	
	if ( RS232_OpenComport( PSPPORT, 19200, "8E1") ){
		printf("Error opening PSP COM port. Exiting...\n");
		RS232_CloseComport(BATPORT);
		exit -1;
	}
	printf("COM ports opened successfully\n");
	
	unsigned char buffer;
	
	while (1){
		
		if( getbatt_byte(&buffer) ){
//			printf("0x%02x in battery buffer", buffer);
			process_message( buffer, &battmsgstate );
			printf("battmsgstate.bufferedchars: %d\n",battmsgstate.bufferedchars);
		}		

		
		if( getpsp_byte(&buffer) ){
//			printf("0x%02x in psp buffer", buffer);
			process_message( buffer, &pspmsgstate );
			printf("pspmsgstate.bufferedchars: %d\n",pspmsgstate.bufferedchars);
		}
	}
		
	RS232_CloseComport(PSPPORT);
	RS232_CloseComport(BATPORT);
}

