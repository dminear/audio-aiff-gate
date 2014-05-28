// reads an .AIFF file and tries to mute the audio when the values fall
// below the given gate value.  Only the first channel is analyzed and
// copied to both channels on the output.
//
// The gain ramps up quickly (in 200 samples) but decays slowly (in 
// 1000 samples).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NDELAYS		910	// number of delay taps (need greater than GATEDELAY)
#define MAXNAMELEN	80		// maximum output filename len
//#define GATEVAL		128	// data value to cut output on
#define GATEDELAY		900	// 900 is about 20.4 mS 		
#define DIGFILLEN		10

#define TRUE			1
#define FALSE			0

struct form
	{
   char label[4];
   unsigned long size;
   char type[4];
   };

struct comm
	{
   char label[4];
   char data[22];
   };   
   
struct ssnd
	{
   char label[4];
   unsigned char size[4];
   char ptr1[4];
   char ptr2[4];		// block
   };
   
double rdelay[NDELAYS];
double ldelay[NDELAYS];

double filter[DIGFILLEN] = {  -0.04388628914739499,
                              -0.09608223054823675,
                              0.04582714594992115,
                              0.3149216128940432,
                              0.45351473922902497,
                              0.0,
                              0.3149216128940432,
                              0.04582714594992115,
                              -0.09608223054823675,
                              -0.04388628914739499,
                              };
   
void main( int argc, char *argv[] )
	{
   FILE 				*infile, *outfile;
   struct form		stform;
   struct comm		stcomm;
   struct ssnd		stssnd;
   unsigned long	datasize, nsamps;
   unsigned long	dotsize;
   unsigned long	loop, dotcount;
   char				bOK = TRUE;
   char				outname[MAXNAMELEN];
	char				*pext;   
   unsigned char	samples[4], outsamples[4];
   size_t			nread;
   int				i;
   char				bGateOn = TRUE;
   int				count = 0;
   double			gateval;
	int				tapval;
   unsigned long	zerolong = 0L;
   unsigned char	lchan[2], rchan[2];
   int				*plchan; //, *prchan;
   double			lout, rout, gain;
   
   
   // clear delay buffers
   for( i = 0; i < NDELAYS; i++)
   	{
      rdelay[i] = 0.0;
      ldelay[i] = 0.0;
      }
	plchan = (int *) lchan;
//   prchan = (int *) rchan;      
   gain = 0.0;
   
   if (argc < 3 )
   	{
	   printf( "gate <filename> <gateval>\n" );
      printf( "Not enough args\n" );
      exit( 1 );
      }
   printf( "File is %s\n", argv[1] );

   gateval = abs( atof( argv[2] ));		// store gate value
   printf( "Gate value is %f.\n", gateval );
   
   // open the file
   if ((infile = fopen( argv[1], "rb" )) == NULL )
   	{
      printf( "Could not open file %s\n", argv[1] );
      exit( 1 );
      }

   // check if it's an AIFF file
	fread( &stform, sizeof( struct form ), 1, infile );
   if (strncmp( stform.label, "FORM", 4 ) != 0)
   	{
      printf( "File %s is not an AIFF file.\n", argv[1] );
      fclose( infile );
      exit( 1 );
      }      

	// next scan in COMM chunk
	fread( &stcomm, sizeof( struct comm ), 1, infile );
   if (strncmp( stcomm.label, "COMM", 4 ) != 0)
   	{
      printf( "File %s does not have a COMM chunk.\n", argv[1] );
      fclose( infile );
      exit( 1 );
      }      

	// read in SSND chunk
	fread( &stssnd, sizeof( struct ssnd ), 1, infile );
   if (strncmp( stssnd.label, "SSND", 4 ) != 0)
   	{
      printf( "File %s does not have a SSND chunk.\n", argv[1] );
      fclose( infile );
      exit( 1 );
      }      
      
   // figure out data size
   datasize = ((unsigned long) stssnd.size[0] << 24)
   				+ ((unsigned long) stssnd.size[1] << 16)
   				+ ((unsigned long) stssnd.size[2] << 8)
   				+ ((unsigned long) stssnd.size[3]);
	printf( "Data length is %lu bytes\n", datasize );    

   // compute number of samples
   nsamps = (datasize - 8) / 2 /*bytes per sample*/ / 2 /*chans*/ ;
   printf( "There are %lu samples in each channel.\n", nsamps );


//fclose(infile);
//exit(1);
   


   
	dotsize =  nsamps / 80;	// number of samples per progress dot

   // open output file with different extension
   strncpy( outname, argv[1], MAXNAMELEN );
   pext = strstr( outname, "." );
   if (pext == NULL)
   	{
      printf( "Source file needs an extension\n" );
      fclose( infile );
      exit( 1 );
      }
	pext++;
   strcpy( pext, "o2" );      
   
   outfile = fopen( outname, "wb" );
   if (outfile == NULL)
   	{
      printf( "Error opening output file %s\n", outname );
      fclose( infile );
      exit( 1 );
      }
   // write out header
	fwrite( &stform, sizeof( struct form ), 1, outfile );
	fwrite( &stcomm, sizeof( struct comm ), 1, outfile );
	fwrite( &stssnd, sizeof( struct ssnd ), 1, outfile );

   // loop through samples
   loop = 0;
   dotcount = 0;
   tapval = NDELAYS - GATEDELAY;
   while ( loop < nsamps && bOK )
   	{
      // read in a sample
      nread = fread( samples, sizeof( samples ), 1, infile );
		if (nread != 1)
      	{
         bOK = FALSE;
         }
      else		// read OK
      	{
         // convert samples
         lchan[0] = samples[1];
         lchan[1] = samples[0];
//         rchan[0] = samples[3];
//         rchan[1] = samples[2];
         ldelay[NDELAYS - 1] = (double) (*plchan); //(int) samples[0] << 8
         							 // + (int) samples[1];
//         rdelay[NDELAYS - 1] = (double) (*prchan); //(int) samples[2] << 8
//         							// + (int) samples[3];
         
         // process samples and adjust gain value
         bGateOn = TRUE;
         // any samples big amplitude?
         for (i = tapval; i < NDELAYS; i++)
         	{
            if (abs(ldelay[i]) > gateval )		// amplitude greater than val
            	{	
               bGateOn = FALSE;		// pass audio
               break;		// no need to continue
               }
            }
         if (bGateOn)
         	{	// attenuate audio slowly
            gain -= 0.0002;
            if (gain < 0.001 )
            	gain = 0.0;
            }
         else	// need to pass audio!
         	{
            // increase gain
            if (gain < 0.995)
	            gain += 0.001;
            }
            
         lout = 0.0;
			// do digital filter
			for( i = 0; i < DIGFILLEN; i++)
				{
				lout += ldelay[tapval+i] * filter[i];
				}
			lout *= gain;		// apply gate gain
            
         // output samples after delay
         if ( count >= GATEDELAY )	// output sample
         	{
            //if (bGateOn)
            //	{
		      //   fwrite( &zerolong, sizeof( zerolong ), 1, outfile );
            //   }               
            //else	// pass input
            	{
               // convert sample	and take from index tapval
               outsamples[0] = ((int) lout >> 8) & 0xFF;
               outsamples[1] = (int) lout & 0xFF;
//               outsamples[2] = ((int) rdelay[tapval] >> 8) & 0xFF;
//               outsamples[3] = (int) rdelay[tapval] & 0xFF;
               outsamples[2] = outsamples[0];	// same as left
               outsamples[3] = outsamples[1];
               if (1 != fwrite( outsamples, sizeof( outsamples ), 1, outfile ))
                  {	// we got a problem
                  bOK = FALSE;
                  printf( "\nCould not write output file.\n" );
                  }
               }
            }
         else	// increase count
         	{
            count++;
            }
         

         // shift samples
         for (i = 0; i < NDELAYS - 1; i++)
         	{
            ldelay[i] = ldelay[i+1];
//            rdelay[i] = rdelay[i+1];
            }
         
         if ( ++dotcount >= dotsize )		// update progress dots
            {
            dotcount = 0;
            printf( "." );
            }
         loop++;		// next sample
         }
      }

   // finish writing last samples
   if (bOK)
   	{
      printf( "Writing tail.\n" );
     	for (i = 0; i < count; i++)
	  		{
   		fwrite( &zerolong, sizeof( zerolong ), 1, outfile );
      	}
      }

	fclose( outfile );      
   fclose( infile );
	}

