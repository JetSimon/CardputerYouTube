from flask import Flask, Response, jsonify, request
from video_server.video_preprocessor import process_videos, get_video_data
import json
import subprocess 
app = Flask(__name__)

FRAME_SIZE = (240, 135)

# process the video files
global video_data
global is_downloading
is_downloading = False
video_data = process_videos("movies", FRAME_SIZE)

@app.route('/search', methods = ['POST'])
def search():
    global is_downloading
    is_downloading = True
    term = request.get_data().decode()
    subprocess.call(f'rm -r cache && rm -r movies && mkdir movies', shell=True)
    subprocess.call(f'yt-dlp ytsearch1:"{term}" -f "worstvideo[ext=mp4]+worstaudio[ext=m4a]/worst[ext=mp4]/worst" --output "movies/video.mp4"', shell=True)
    global video_data
    video_data = process_videos("movies", FRAME_SIZE)
    is_downloading = False
    return json.dumps({'success':True}), 200, {'ContentType':'application/json'}

@app.route('/channel_info')
def get_channel_lengths():
    lengths = [len(audio) for audio, frames in video_data]
    return jsonify(lengths)

@app.route('/audio/<int:channel_index>/<int:start>/<int:length>')
def get_audio(channel_index, start, length):
    global is_downloading
    if is_downloading:
        return Response([], mimetype='audio/x-raw')
    audio, frames = video_data[channel_index % len(video_data)]
    end = start + length
    if end > len(audio):
        end = len(audio)
    if start > end:
        start = end
    if start == end:
        # return nothing - we've got no more data to give
        return Response(b'', mimetype='audio/x-raw')
    slice = audio[start:end]
    return Response(slice, mimetype='audio/x-raw')

@app.route('/frame/<int:channel_index>/<int:ms>')
def get_frame(channel_index, ms):
    global is_downloading
    if is_downloading:
        return json.dumps({'success':False}), 404, {'ContentType':'application/json'}
    audio, frames = video_data[channel_index % len(video_data)]
    # use binary search to find the closest frame
    start = 0
    end = len(frames) - 1
    while start <= end:
        mid = (start + end) // 2
        if frames[mid][0] == ms:
            return Response(frames[mid][1], mimetype='image/jpeg')
        elif frames[mid][0] < ms:
            start = mid + 1
        else:
            end = mid - 1
    # we may not find the exact frame, so return the closest frame
    if end < 0:
        end = 0
    elif start >= len(frames):
        start = len(frames) - 1
    return Response(frames[start][1], mimetype='image/jpeg')

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8123)
