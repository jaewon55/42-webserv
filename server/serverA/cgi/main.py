#!/usr/bin/env python3

import os
import sys
import cgi
import uuid
from datetime import datetime
import urllib.parse
import mimetypes
from http import cookies
import json

def get_mime_type(file_path):
    mime_type, _ = mimetypes.guess_type(file_path)
    return mime_type

def get_file_extension(filename):
    dot_index = filename.rfind('.')
    if dot_index == -1:
        return ''
    return filename[dot_index:]

def get_response_body(content):
    response_body = '''<!DOCTYPE html>
    <html lang="ko">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link rel="stylesheet" href="/style">
        <title>Gallery</title>
    </head>
    <script>
        function deleteImage(imageName) {
            fetch(`/delete?image=${imageName}`, {
                method: 'DELETE',
            })
            .then((response) => {
                if (response.ok) {
                    location.reload();
                } else {
                    alert('Failed to delete the image!');
                }
            })
            .catch((error) => {
                console.error('Error:', error);
            });
        }

        function toggleLike(e) {
            const element = e.target;

            if (element.className === "like") {
                fetch(`/like?image=${element.id}`, {
                    method: 'GET',
                })
                .then((response) => {
                    if (response.ok) {
                        console.log('like');
                        element.classList.toggle("like");
                        element.classList.toggle("liked");
                    } else {
                        alert('Failed to like!');
                    }
                })
                .catch((error) => {
                    console.error('Error:', error);
                });
            } else if (element.className === "liked") {
                fetch(`/unlike?image=${element.id}`, {
                    method: 'GET',
                })
                .then((response) => {
                    if (response.ok) {
                        console.log('unlike');
                        element.classList.toggle("like");
                        element.classList.toggle("liked");
                    } else {
                        alert('Failed to unlike!');
                    }
                })
                .catch((error) => {
                    console.error('Error:', error);
                });
            }
        }
    </script>'''
    
    response_body += f'''<body>
        <canvas id="canv"></canvas>
        <div id="nav">
            <h1>list</h1>
            <div>
                <h3><a href="/">home</a></h3>
                <h3><a href="/upload">upload</a></h3>
                <h3><a href="/list">list</a></h3>
                <h3><a href="/intra" target="_blank">intra</a></h3>
            </div>
        </div>
        <div id="content">
            {content}
        </div>
        <script type="text/javascript" src="/js"></script>
    </body>
    </html>'''

    return response_body

def request(uri, method, body, query, cookie):
    directory = "./server/serverA/storage"
    response_body = ""

    session_id = ""
    if cookie == None or cookie['session_id'].value == None:
        session_id = str(uuid.uuid4())
    else:
        session_id = cookie['session_id'].value

    if "/list" == uri and "GET" == method:
        # storage에 있는 파일 목록 반환, 목록 클릭 시 get요청으로 쿼리스트링 보냄

        filename = f"{directory}/sessions.json"
        data = {}
        # 파일이 존재하고 비어 있지 않은지 확인합니다.
        if os.path.exists(filename) and os.path.getsize(filename) > 0:
            with open(filename, "r") as file:
                content = file.read()
                # JSON 데이터를 파싱하려 시도합니다.
                if content.strip():  # 문자열이 공백이 아닌지 확인합니다.
                    data = json.loads(content)

        data.setdefault(session_id, {})

        # 이미지 확장자 목록
        image_extensions = {".jpg", ".jpeg", ".png"}

        list_html = ""
        # 디렉토리에 있는 파일 목록 가져오기
        with os.scandir(directory) as entries:
            for entry in entries:
                if entry.is_file():
                    _, extension = os.path.splitext(entry.name)
                    if extension.lower() in image_extensions:
                        data[session_id].setdefault(entry.name, 'like')

                        list_html += f'''
                        <div class="imageBox">
                            <button class="{data[session_id][entry.name]}" id="{entry.name}" onclick="toggleLike(event)">Like</button>
                            <div class="imgWrap">
                                <img src="/picture?image={entry.name}" alt="{entry.name}">
                            </div>
                            <div class="buttonBox">
                                <a href="/picture?image={entry.name}" download><button class="downloadButton">Download</button></a>
                                <button class="deleteButton" onclick="deleteImage('{entry.name}')">Delete</button>
                            </div>
                        </div>
                        '''
        
        with open(f"{directory}/sessions.json", "w") as file:
            json.dump(data, file)

        response_body = get_response_body(list_html)
        
        # response 만들기
        response_string = "Status: 200 OK\r\n"
        response_string += "Content-Type: text/html\r\n"
        response_string += f"Set-Cookie: session_id={session_id}; Max-Age=3600; HttpOnly\r\n"
        response_string += f"Content-Length: {len(response_body)}\r\n\r\n"
        response_string += response_body
        sys.stdout.write(response_string)
        exit(0)

    elif "/picture" == uri and "GET" == method:
        # 쿼리스트링으로 들어온 값에 해당하는 사진 보여주기
        # 다운로드 버튼 필요
        src = ""

        if query["image"][0] is not None:
            src = f"{directory}/{query['image'][0]}"
            with open(src, "rb") as image_file:
                image_data = image_file.read()
                # response 만들기
                response_string = "Status: 200 OK\r\n"
                response_string += f"Content-Type: {get_mime_type(src)}\r\n"
                response_string += f"Content-Length: {len(image_data)}\r\n\r\n"
                
                sys.stdout.write(response_string)
                sys.stdout.flush()  # 버퍼를 비워줍니다.
                sys.stdout.buffer.write(image_data)
                sys.stdout.buffer.flush() 
                exit(0)
        else:
            error_response(404, "No image file in storage.")
    
    elif "/save" == uri and "POST" == method:
        if body['image'] is not None:
            
            # 'image' 필드에 업로드된 파일 정보를 가져옵니다.
            image_file = body['image']
            if type(image_file) == list:
                image_file = image_file[1]

            # 업로드된 파일의 이름을 확인합니다.
            uploaded_filename = image_file.filename
            ext = get_file_extension(uploaded_filename)
            
            # 고유한 파일명 생성
            unique_filename = f"{uuid.uuid4().hex}{ext}"
            
            # 이미지 저장 경로 생성
            save_path = os.path.join("./server/serverA/storage", unique_filename)
            
            # 이미지를 로컬 스토리지에 저장
            with open(save_path, 'wb') as outfile:
                outfile.write(image_file.file.read())

            # response 만들기
            response_string = "Status: 303 See Other\r\n"
            response_string += "Content-length: 0\r\n"
            response_string += "Location: /list\r\n\r\n"
            
        else:
            # response 만들기
            response_string = "Status: 400 Bad Request\r\n"
            
        sys.stdout.write(response_string)
        exit(0)

    elif "/delete" == uri and "DELETE" == method:
        if query["image"][0] is not None:
            src = f"{directory}/{query['image'][0]}"
            os.unlink(src)
            
            # response 만들기
            response_string = "Status: 200 OK\r\n"
            response_string += "Content-Type: text/plane; charset=UTF-8\r\n"
            response_string += "Content-Length: 0\r\n\r\n"
            
            sys.stdout.write(response_string)
            exit(0)
        else:
            error_response(400, "Request Error.")
        

    elif "/like" == uri and "GET" == method:
        if query["image"][0] is not None:
            image_name = query['image'][0]
            
            filename = f"{directory}/sessions.json"
            data = {}
            # 파일이 존재하고 비어 있지 않은지 확인합니다.
            if os.path.exists(filename) and os.path.getsize(filename) > 0:
                with open(filename, "r") as file:
                    content = file.read()
                    # JSON 데이터를 파싱하려 시도합니다.
                    if content.strip():  # 문자열이 공백이 아닌지 확인합니다.
                        data = json.loads(content)
            
            data.setdefault(session_id, {})
            data[session_id][image_name] = 'liked'
            
            with open(f"{directory}/sessions.json", "w") as file:
                json.dump(data, file)

            # response 만들기
            response_string = "Status: 200 OK\r\n"
            response_string += "Content-Type: text/plane;\r\n"
            response_string += "Content-Length: 0\r\n\r\n"
            sys.stdout.write(response_string)
            exit(0)
        else:
            error_response(400, "Request Error.")
    
    elif "/unlike" == uri and "GET" == method:
        if query["image"][0] is not None:
            image_name = query['image'][0]
            
            filename = f"{directory}/sessions.json"
            data = {}
            # 파일이 존재하고 비어 있지 않은지 확인합니다.
            if os.path.exists(filename) and os.path.getsize(filename) > 0:
                with open(filename, "r") as file:
                    content = file.read()
                    # JSON 데이터를 파싱하려 시도합니다.
                    if content.strip():  # 문자열이 공백이 아닌지 확인합니다.
                        data = json.loads(content)
            
            data.setdefault(session_id, {})
            data[session_id][image_name] = 'like'
            
            with open(f"{directory}/sessions.json", "w") as file:
                json.dump(data, file)
                
            # response 만들기
            response_string = "Status: 200 OK\r\n"
            response_string += "Content-Type: text/plane;\r\n"
            response_string += "Content-Length: 0\r\n\r\n"
            sys.stdout.write(response_string)
            exit(0)

    else:
        error_response(400, "Request Error.")

def error_response(state, message):
    response_string = ""

    if state == 400:
        response_string += "Status: 400 Bad Request\r\n"
    elif state == 404:
        response_string += "Status: 404 Not Found\r\n"
    elif state == 405:
        response_string += "Status: 405 Method Not Allowed\r\n"
    elif state == 500:
        response_string += "Status: 500 Internal Server Error\r\n"
    elif state == 501:
        response_string += "Status: 501 Not Implemented\r\n"
    elif state == 505:
        response_string += "Status: 505 HTTP Version Not Supported\r\n"
    else:
        response_string += "Status: 418 I'm a teapot\r\n"

    response_string += f"Content-Type: text/plain\r\nContent-Length: {len(message)}\r\n\r\n"
    response_string += message
    
    sys.stdout.write(response_string)
    sys.exit(1)

def main():
    try:
        # 환경 변수에서 요청 정보 가져오기
        request_method = os.environ.get("REQUEST_METHOD")
        query_string = os.environ.get("QUERY_STRING")
        uri = os.environ.get("PATH_INFO")
        content_type = os.environ.get("CONTENT_TYPE")
        http_cookie = os.environ.get("HTTP_COOKIE")

        if http_cookie != "":
            # SimpleCookie 객체 생성
            cookie = cookies.SimpleCookie()
            # 쿠키 텍스트를 파싱하여 객체에 로드
            cookie.load(http_cookie)
        else:
            cookie = None

        if request_method == 'GET' or request_method == 'DELETE':
            query = urllib.parse.parse_qs(query_string)
            request(uri, request_method, {}, query, cookie)
        
        elif request_method == 'POST':
            # content_type이 리스트 내에 있는지 확인
            if content_type is None or not any([ct in content_type for ct in ["multipart/form-data"]]):
                error_response(418, "drink some tea!")
            else:
                # 멀티파트 폼 데이터 처리
                if "multipart/form-data" in content_type:
                    os.environ["CONTENT_TYPE"] = content_type
                    body = cgi.FieldStorage()
                    
                    request(uri, request_method, body, {}, cookie)
                    
        else:
            error_response(405, f"{request_method} is not allowed method.")

    except Exception as e:
        # 예외 처리
        sys.stderr.write(f"python error: {str(e)}\n")
        error_response(500, f"python error: {str(e)}")

main()
