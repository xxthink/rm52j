/******************************************************************
   		 AVS Reference Software Manual
******************************************************************/
please send comments and additions to jianwen.chen@gmail.com

1. Compilation
2. Command line parameters
3. Input/Output file format
4. Configuration files


****************************************************************
1. Compilation

1.1 Windows
  
  A workspace for MS Visual C++ is provided with the name "RM.dsw". It contains
  the encoder and decoder projects.  And you can also use the separate project files:
  lencod.dsw , ldecod.dsw.

1.2 Unix

    Makefiles are provided in the lencod and ldecod directory.
    'make' comand will creat the obj directory and generate the executable file in 
    the 'bin' directory.
*******************************************************************

2. Command line parameters

  2.1 Encoder

      lencod.exe [-f file] [-p parameter=value]

     All Parameters are initially taken from the 'file ', typically: "encoder.cfg"

     -f file    
             If an -f <config> parameter is present in the command line then 
             the parameters will be taken from the config file
             See configfile.h for a list of supported ParameterNames.

    -p parameter=value 
             If -p <ParameterName = ParameterValue> parameters are present then 
             the ParameterValue will overide the config file's settings.
            There must  be whitespace between -f and -p commands and their respecitive 
            parameters.

  2.2 Decoder

    ldecod.exe decoder.cfg

   The decoder configuration file name must be provided as the first parameter. All
   decoding parameters are read from this file.

*******************************************************************
3. Input/Output file format

   The codec can only support 4:2:0 format video sequences. 
   For encoder, the input files should be the 4:2:0 data files and the output is *.avs 
   which is the stream of AVS standard.
   For decoder, the input files should be the AVS standard streams and the output is 
   the 4:2:0 video data.

*******************************************************************
4. Configuration files

   Sample encoder and decode configuration files are provided in the bin/ directory.
   These contain explanatory comments for each parameter.
  
4.1 Encoder
   <ParameterName> = <ParameterValue> # Comments

   Whitespace is space and \t

   <ParameterName>  are the predefined names for Parameters and are case sensitive.
                   See configfile.h for the definition of those names and their 
                   mapping to configinput->values.

   <ParameterValue> are either integers [0..9]* or strings.
                  Integers must fit into the wordlengths, signed values are generally 
                  assumed. Strings containing no whitespace characters can be used directly.
                  Strings containing whitespace characters are to be inclosed in double 
                  quotes ("string with whitespace")

    If the  ParameterName is undefined, the program will be terminated with an error message.

4.2 Decoder
  <value>    #comment

  The values are read in a predefined order. See the example file for details.

*******************************************************************
