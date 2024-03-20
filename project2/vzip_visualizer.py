import zlib
import cv2 as cv
import numpy

#Open the video
video = open("video.vzip", "rb")

#Variable definitions
i = 0
frameSizes = []
decompressedFrames = []

#Loop through all the frames in the file and decompress them
while True:
    #Get the size of the current frame as an integer by assuming that every 4 bytes before a frame is that frame's size
    frameSizes.append(int.from_bytes(video.read(4), byteorder = 'little'))
    #The end of the file is reached when the frame size is 0
    if frameSizes[i] == 0: break

    #Get the compressed frame by reading the entire frame using its size, then decompress it
    compressedFrame = video.read(frameSizes[i])
    decompressedFrame = zlib.decompress(compressedFrame)

    #Turn the decompressed frame from a byte type object to a numpy array that represents the pixels values in the frame
    decompressedFrame = numpy.frombuffer(decompressedFrame, dtype = numpy.uint8)
    #Then, decode the numpy array into an opencv color image
    decompressedFrame = cv.imdecode(decompressedFrame, cv.IMREAD_COLOR)
    decompressedFrames.append(decompressedFrame)

    i += 1

video.close()

#Show every frame in an opencv window, and pause between each frame to make the video play at about 30fps
for frame in decompressedFrames:
    cv.imshow("Video", frame)
    cv.waitKey(int(1000/30))
cv.destroyAllWindows()