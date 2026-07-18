import os
import requests
import time
from PIL import Image

# Điền thông tin API của bạn
API_KEY = "TVOlPyMlopjjxFd1KRXVF0e2na6IlIll"
API_SECRET = "lHmbOhuIjFtqw2mXa5OunKLQ9FHDiAEb"
FACESET_OUTER_ID = "registered_face" # Tên nhóm ảnh

REGISTERED_DIR = "registered_face"

def safe_post(url, data=None, files=None, max_retries=3):
    for attempt in range(max_retries):
        try:
            res = requests.post(url, data=data, files=files, timeout=10)
            if res.status_code == 200:
                return res
            elif res.status_code == 403:
                # Rate limit or quota exceeded
                print(f"Lỗi 403 (Có thể quá giới hạn API), thử lại sau 3s... Lần {attempt+1}")
                time.sleep(3)
            else:
                return res
        except Exception as e:
            print(f"Lỗi kết nối ({e}), đang thử lại... Lần {attempt+1}")
            time.sleep(2)
    return None

# Tạo một FaceSet (Album) mới trên Cloud Face++
print("Đang tạo hoặc kiểm tra album FaceSet trên Cloud...")
create_url = "https://api-us.faceplusplus.com/facepp/v3/faceset/create"
safe_post(create_url, data={
    'api_key': API_KEY,
    'api_secret': API_SECRET,
    'outer_id': FACESET_OUTER_ID,
    'display_name': "My Home Faces"
})

# Hàm nén ảnh nếu dung lượng quá lớn (> 1.5MB)
def compress_image(img_path):
    if os.path.getsize(img_path) > 1.5 * 1024 * 1024:
        with Image.open(img_path) as img:
            new_size = (img.width // 2, img.height // 2)
            resized_img = img.resize(new_size, Image.Resampling.LANCZOS)
            resized_img.save(img_path, optimize=True, quality=85)

detect_url = "https://api-us.faceplusplus.com/facepp/v3/detect"
add_url = "https://api-us.faceplusplus.com/facepp/v3/faceset/addface"
setuserid_url = "https://api-us.faceplusplus.com/facepp/v3/face/setuserid"

face_tokens_list = []

print("Bắt đầu quét ảnh trong thư mục và upload...")

# Quét qua các thư mục con trong registered_face (tên thư mục là tên người)
for person_name in os.listdir(REGISTERED_DIR):
    person_dir = os.path.join(REGISTERED_DIR, person_name)
    
    if os.path.isdir(person_dir):
        # Mã hóa tên tiếng Việt có dấu sang chuỗi Hex (an toàn cho API Face++)
        hex_user_id = person_name.encode('utf-8').hex()
        print(f"\n>> Đang xử lý thành viên: {person_name} (Mã nội bộ: {hex_user_id})")
        
        for file_name in os.listdir(person_dir):
            if file_name.lower().endswith(('.png', '.jpg', '.jpeg')):
                img_path = os.path.join(person_dir, file_name)
                
                try:
                    compress_image(img_path)
                except Exception as e:
                    print(f" Lỗi khi nén ảnh {file_name}: {e}")
                    continue
                
                with open(img_path, "rb") as img_file:
                    # 1. Phát hiện khuôn mặt và lấy token
                    response = safe_post(detect_url, data={'api_key': API_KEY, 'api_secret': API_SECRET}, files={'image_file': img_file})
                    
                    if response and response.status_code == 200:
                        data = response.json()
                        if len(data.get('faces', [])) > 0:
                            token = data['faces'][0]['face_token']
                            
                            # 2. Gán mã hex_user_id (tên thành viên) vào khuôn mặt này
                            user_res = safe_post(setuserid_url, data={
                                'api_key': API_KEY,
                                'api_secret': API_SECRET,
                                'face_token': token,
                                'user_id': hex_user_id
                            })
                            
                            if user_res and user_res.status_code == 200:
                                face_tokens_list.append(token)
                                print(f"  + Thành công: {file_name} -> Nhận dạng là {person_name}")
                            else:
                                print(f"  - Lỗi khi gán tên cho {file_name}: {user_res.text}")
                        else:
                            print(f"  - Không tìm thấy mặt trong ảnh: {file_name}")
                    else:
                        print(f"  - Lỗi API khi quét {file_name}: {response.text}")
                
                time.sleep(0.5)

# 3. Thêm tất cả các token vào FaceSet
if face_tokens_list:
    print(f"\nĐang thêm tổng cộng {len(face_tokens_list)} khuôn mặt vào album Cloud...")
    
    chunk_size = 5
    success_count = 0
    
    for i in range(0, len(face_tokens_list), chunk_size):
        chunk = face_tokens_list[i:i + chunk_size]
        tokens_str = ",".join(chunk)
        
        add_res = safe_post(add_url, data={
            'api_key': API_KEY,
            'api_secret': API_SECRET,
            'outer_id': FACESET_OUTER_ID,
            'face_tokens': tokens_str
        })
        
        if add_res and add_res.status_code == 200:
            success_count += len(chunk)
            print(f" Đã thêm nhóm {len(chunk)} khuôn mặt vào Album...")
        else:
            print("Lỗi khi gom ảnh vào Album:", add_res.text)
        
        time.sleep(0.5)
        
    print(f"\n=== TRAIN THÀNH CÔNG {success_count} KHUÔN MẶT ĐÃ GẮN TÊN! ===")
else:
    print("\nKhông tìm thấy khuôn mặt nào hợp lệ để train!")
