# Content-aware Video Encoding for Cloud Gaming

## Quality Evaluation

### Folder Structure. 
A folder containing a video sequence to be evaluated in terms of video quality metrics (e.g., SSIM, VMAF, ...) should have a special structure to run with the provided evaluation code.

#### Folder Name. 

A video sequence should be placed in a folder called "ga< sequence number >" (the sequence number 
concatenated to ga).
#### Sequence Name. 

The video sequence file should have the following name: raw_< width >_< height >.yuv
#### Folder Contents. 

The folder should contain a sub-directory called QP to hold the encoded versions of the video sequence under 4 different QPs. In addition a folder called temp should be present to hold the evaluation results of the encoded sequences. The folder should contain a text file called conf.txt which contains key-value
pairs as follows:
- width=< video width >
- height=< video height >
- fps=< frames per second >

An example of the folder structure should look as follows:
- ga1\
  - QP\
  - temp\
  - raw_1280_720.yuv
  - conf.txt

### Running the Evaluation Code. 

The following dependencies are needed:
- Windows 7
- Linux CentOS
- Visual Studio 2017
- Matlab
- OpenCL-enabled GPU2
- Python
- Java

The source code directory contains the following folders:
- YUVPlayerROI: this folder contains a Java project responsible for defining ROIs manually. The YUV conversion is taken from [here](https://github.com/luuvish/java-yuv-viewer). The program opens a GUI to select a video file with YUV format and allows for defining ROIs using the format defined [here](https://github.com/AlexeyAB/darknet#how-to-train-to-detect-your-custom-objects). By default, the ROIs are defined for every other frame in the video. There are multiple ROI types with different importance factors which can be edited through the source file YUVPlayer.java. By default, the ROIs will be written in a file besides the location of the video sequence (i.e., inside the ga< sequence number > folder). The ROI files
should not be moved outside of this directory.
- Encoder: this folder contains the source code responsible for encoding a raw video file under different methods (i.e., Base, CAVE, RQ). The folder contains a Visual Studio 2017 solution. Inside the debug directory, there exists a python script called encode.py which drives the encoding process of the various video sequences. This script contains variables that should be changed to evaluate new sequences. These
variables are:
  - base_path at line 4: This path should be changed to point at the parent directory containing the folders of the video sequences (i.e., the folder containing the ga< sequence number > folders).
  - K at line 6: This is a 2D array that contains the K value at each bitrate for each video sequence. The rows of this 2D array represent the video sequence and the columns represent the bitrate. For example, at the highest bitrate for the first video sequence we can have a K value of 7, therefore the entry (0,0) of this 2D array would be 7.
  - length at line 7: This is the length of the video sequence
in seconds. This value should be changed to the length of
the video sequence and assuming that all video sequences
have the same length.
  - width at line 11: This is a 1D array holding the width of
each video sequence.
  - height at line 12: This is a 1D array holding the height of
each video sequence.
  - range at line 14: This is the range of the video sequences
numbers. For example, if the video sequences are as follows: ga4, ga5, ga6, then the range should be changed
to become (4,7).

  The encode.py script calls another python script called qp.py to encode a sequence under 4 different QPs at line 16. Then the encoder.py script calls the Encoder.exe which takes the following options in order:

  - path: This is the path to the video sequence folder (i.e., ga< sequence number >).
  - bitrate: This is the target number of bits per second.
  - method: This is the method to use for encoding which takes the following values: 0 for CAVE, 2 for RQ, and 4 for Base.
  - sequence number: This is sent as 1 by default.
  - K: This is the K value used for CAVE and is valid when the method to encode with is set to CAVE only and should be sent as 0 value otherwise.
  - encoder: This is the index encoder to use and should be sent as 0 by default to use the x265 encoder.

- Evaluation: this folder contains the Matlab scripts required to evaluate the encoded videos. Before running the evaluation, VMAF should be downloaded and compiled (C++) for Linux as noted [here](https://github.com/Netflix/vmaf). The evaluation code should run on a Linux machine, by running the run.sh script. This script will decode the encoded sequences in their directory and evaluate their VMAF score. Then the script will call the Matlab script called evaluationsVariations.m to evaluate the sequences under different methods. The evaluation scripts should be placed inside the folder containing the video sequences to be evaluated. The following variables should be changed in runs.sh to evaluate new sequences: 

  - games at line 2: This is a 1D array containing the name of the folders of the video sequences (e.g., ga1, ga2, . . . ).
  - width at line 5: This is a 1D array holding the width of each video sequence.
  - height at line 6: This is a 1D array holding the height of each video sequence.

  The following variables should be changed in evaluationsV ariations.m to evaluate new sequences:
  - width_ at line 19: This is a 1D array holding the width of each video sequence.
  - height_ at line 20: This is a 1D array holding the height of each video sequence.
  - base_path_ at line 26: This is a 1D array containing the name of the folders of the video sequences (e.g., ga1, ga2, . . . ).
  
  Lines 5, 6, 7 in the Matlab script can be commented out if multiple cores do not exist on the machine.

## Overhead Evaluation
The gaminganywhere folder contains the implementation of CAVE in GamingAnwyhere. The preferred environment to run this code is by using Windows 7, Visual Studio 2010 and an OpenCL-enabled GPU if the RQ method is to be tested. The run time overhead of CAVE will be printed in the log file maintained by GamingAnywhere. Therefore, when running a specific game logging should be enabled by specifying a path to the log file. A sample of a configuration file containing the added configuration needed by CAVE is found under gaminganywhere/bin.win32/config/server.d3de x-rc.conf which has 4 new added variables at the end of the file:

- mode: This is the mode to run GamingAnywhere under which can take a value of 0 for CAVE and 2 for RQ and 3 for Base.
- K: This is the K value used for CAVE and is valid only when the mode is set to CAVE.
- recording: This is a boolean variable used to store raw
frames in a file. The raw file will be stored with the Game executable if the mode of GamingAnwywhere is event-driven, otherwise it will be stored with the executable of the server.

Two common configuration files are placed inside the common directory for the configuration of the x265 encoder and are called video-x265-param-rc.conf and video-x265-rc.conf. The directory containing the executable of the server and the directory of the executable of the game should contain a file called roi0.txt to hold default ROI information for CAVE with the same format as used in the quality evaluation.

## Miscellaneous

The source code directory contains a folder called Unity-ROI which contains the example of the game developed in Unity to show the ability to extract ROIs from a real game engine. The code is instrumented to take a screenshot of each frame and store it besides the solution. The requirements to run this code are Unity 5.5 and Visual Studio 2017.

**__Note: “Subject to open source code identified, the code in this package was written solely by researchers at SFU”__**