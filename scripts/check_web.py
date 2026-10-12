"""Optional browser regression test: requires Python playwright + its Chromium browser."""
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import gzip
import json
import re
import threading
from playwright.sync_api import sync_playwright

ROOT=Path(__file__).resolve().parent.parent
PAGE=(ROOT/'web/index.html').read_bytes()
header=(ROOT/'include/web_dashboard.h').read_text()
embedded=bytes(int(byte,16) for byte in re.findall(r'0x([0-9a-f]{2})',header))
assert gzip.decompress(embedded)==PAGE, 'Embedded asset differs from HTML source'
state={'offline':False,'fault':False,'zero':False,'weather_http':200,'posts':0,'weather_code':804,'stale':False,'omit_lights':False}
provider=json.loads((ROOT/'tests/fixtures/weather.json').read_text())
air=json.loads((ROOT/'tests/fixtures/air.json').read_text())

class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):
        pass
    def send_payload(self,code,payload,kind='application/json'):
        data=json.dumps(payload).encode() if kind=='application/json' else payload
        self.send_response(code);self.send_header('Content-Type',kind)
        if kind=='text/html; charset=utf-8':self.send_header('Content-Encoding','gzip')
        self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
    def do_GET(self):
        if self.path=='/':self.send_payload(200,embedded,'text/html; charset=utf-8');return
        if state['offline']:self.send_payload(503,{'error':'offline'});return
        if self.path=='/api/sensors':
            ready=not state['fault'];self.send_payload(200,{
                'dht':{'model':22,'pin':27,'valid':ready,'status':'ready' if ready else 'unavailable',
                       'temperature_c':(0 if state['zero'] else 26.5) if ready else None,'relative_humidity_percent':(0 if state['zero'] else 61) if ready else None,'age_ms':500},
                **({} if state['omit_lights'] else {'ldr':{'valid':True,'raw_adc':700,'millivolts':700,'resistance_ohms':12345},
                                                     'rgb':{'enabled':True,'led_color':'GREEN'}})})
        elif self.path=='/api/status':
            self.send_payload(200,{'firmware_version':'1.2.0','ipv4':'192.168.1.123','heap_free_bytes':123456,
                                 'heap_min_free_bytes':100000,'weather_stack_min_free_bytes':4096})
        elif self.path.startswith('/api/weather?'):
            cod=state['weather_http'];name='Bangkok' if 'bangkok' in self.path else 'Chiang Mai'
            self.send_payload(cod,{'pending':cod==202,'error':'' if cod==200 else 'HTTP 401: API key invalid/not active' if cod==503 else '',
                'status':{'weather_available':cod==200,'air_available':cod==200,'weather_stale':state['stale'],'air_stale':False,
                          'weather_data_time_utc':provider['dt'] if cod==200 else 0,'air_data_time_utc':air['list'][0]['dt'] if cod==200 else 0},
                'location':{'name':name,'latitude':13.7563 if name=='Bangkok' else 18.7883,'longitude':100.5018,'utc_offset_seconds':25200,'resolved':True,'approximate':False},
                'weather':{'temperature_c':provider['main']['temp'],'relative_humidity_percent':provider['main']['humidity'],'code':state['weather_code'] if cod==200 else None,'description':'Clouds'},
                'rain_cloud':{'rain_rate_mm_per_hour':provider.get('rain',{}).get('1h',0),'cloud_cover_percent':provider['clouds']['all']},
                'wind':{'speed_kmh':provider['wind']['speed']*3.6,'from_compass':['N','NNE','NE','ENE','E','ESE','SE','SSE','S','SSW','SW','WSW','W','WNW','NW','NNW'][int((provider['wind'].get('deg',0)+11.25)/22.5)%16],'from_degrees':provider['wind'].get('deg')},
                'air_quality':{'pm2_5_ug_m3':air['list'][0]['components']['pm2_5']}})
        else:self.send_payload(404,{'error':'missing'})
    def do_POST(self):
        state['posts']+=1
        self.send_payload(405,{'error':'Web dashboard should not send control commands'})

server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    with sync_playwright() as playwright:
        browser=playwright.chromium.launch()
        page=browser.new_page(viewport={'width':1280,'height':1000},timezone_id='Asia/Bangkok')
        requests=[];page.on('request',lambda request:requests.append(request.url))
        errors=[];page.on('pageerror',lambda error:errors.append(str(error)))
        page.goto(f'http://127.0.0.1:{server.server_port}/')
        page.wait_for_function("document.getElementById('dht-temp').textContent==='26.5'")
        page.wait_for_function("document.getElementById('weather-temp').textContent!=='—'")
        assert page.locator('[id^="ldr-"]').count()==0
        assert 'LDR' not in page.locator('body').inner_text()
        assert page.locator('[id^="rgb-"]').count()==0
        assert 'RGB' not in page.locator('body').inner_text()
        assert page.locator('#weather-icon').get_attribute('data-condition')=='overcast'
        assert page.locator('#weather-condition').inner_text()=='เมฆเต็มท้องฟ้า'
        state['omit_lights']=True
        page.evaluate('async()=>renderSensors(await request("/api/sensors"))')
        assert page.locator('#dht-temp').inner_text()=='26.5'
        cases=[(200,'thunderstorm','พายุฝนฟ้าคะนอง'),(321,'drizzle','ฝนละออง'),
               (500,'rain','ฝนเล็กน้อย'),(501,'rain','ฝนปานกลาง'),(502,'heavy-rain','ฝนหนัก'),
               (503,'heavy-rain','ฝนหนักมาก'),(504,'heavy-rain','ฝนรุนแรง'),(511,'freezing-rain','ฝนเยือกแข็ง'),
               (520,'rain','ฝนโปรย'),(521,'rain','ฝนตกเป็นช่วง'),(522,'heavy-rain','ฝนหนักเป็นช่วง'),
               (531,'rain','ฝนตกเป็นช่วงไม่สม่ำเสมอ'),(622,'snow','หิมะตก'),(701,'mist','หมอกบาง'),
               (711,'smoke','ควัน'),(721,'haze','หมอกควัน'),(731,'dust','ฝุ่นทรายฟุ้ง'),(741,'mist','หมอกหนา'),
               (751,'sand','ทราย'),(761,'dust','ฝุ่น'),(762,'ash','เถ้าภูเขาไฟ'),(771,'squalls','ลมกระโชกแรง'),
               (781,'tornado','พายุทอร์นาโด'),(800,'clear','ท้องฟ้าแจ่มใส'),(801,'few-clouds','มีเมฆเล็กน้อย'),
               (802,'scattered-clouds','เมฆกระจาย'),(803,'broken-clouds','มีเมฆเป็นส่วนมาก'),
               (804,'overcast','เมฆเต็มท้องฟ้า'),(999,'unknown','ไม่ทราบสภาพอากาศ'),(None,'unknown','ไม่ทราบสภาพอากาศ')]
        for code,icon,label in cases:
            state['weather_code']=code
            page.evaluate('loadWeather()')
            assert page.locator('#weather-icon').get_attribute('data-condition')==icon, code
            assert page.locator('#weather-icon').locator('path,circle').count()>0, code
            assert page.locator('#weather-condition').inner_text()==label, code
        state['weather_code']=200;state['stale']=True
        page.evaluate('loadWeather()')
        assert 'ข้อมูลเก่า' in page.locator('#weather-state').inner_text()
        assert page.locator('#weather-icon').get_attribute('data-condition')=='thunderstorm'
        state['stale']=False;state['weather_code']=804
        page.select_option('#location','bangkok')
        page.wait_for_function("document.getElementById('weather-position').textContent.includes('13.7563')")
        state['fault']=True
        page.wait_for_function("document.getElementById('dht-temp').textContent==='—'")
        assert 'ไม่มีอุปกรณ์' in page.locator('#sensor-note').inner_text()
        state['fault']=False;state['zero']=True
        page.wait_for_function("document.getElementById('dht-temp').textContent==='0.0'")
        assert page.locator('#dht-humidity').inner_text()=='0.0'
        state['zero']=False
        state['weather_http']=202;page.locator('#weather-reload').click()
        page.wait_for_function("document.getElementById('weather-state').textContent.includes('กำลังอัปเดต')")
        assert page.locator('#weather-temp').inner_text()=='—'
        assert page.locator('#weather-icon').get_attribute('data-condition')=='unknown'
        assert page.locator('#weather-condition').inner_text()=='กำลังโหลดข้อมูลอากาศ…'
        state['weather_http']=503;page.locator('#weather-reload').click()
        page.wait_for_function("document.getElementById('weather-state').textContent.includes('HTTP 401')")
        assert page.locator('#weather-condition').inner_text()=='ยังไม่มีข้อมูลอากาศ'
        state['weather_http']=200;page.locator('#weather-reload').click()
        page.wait_for_function("document.getElementById('weather-temp').textContent!=='—'")
        page.screenshot(path='/tmp/cyd-web-desktop.png',full_page=True)
        page.set_viewport_size({'width':390,'height':844})
        assert page.evaluate('document.documentElement.scrollWidth<=window.innerWidth'), 'Horizontal overflow on mobile'
        page.screenshot(path='/tmp/cyd-web-mobile.png',full_page=True)
        page.set_viewport_size({'width':320,'height':740})
        assert page.evaluate('document.documentElement.scrollWidth<=window.innerWidth'), 'Horizontal overflow on narrow mobile'
        state['offline']=True
        page.wait_for_function("document.getElementById('connection').classList.contains('bad')")
        assert page.locator('#dht-temp').inner_text()=='—'
        page.evaluate('loadWeather()')
        assert page.locator('#weather-icon').get_attribute('data-condition')=='unknown'
        assert page.locator('#weather-condition').inner_text()=='ไม่สามารถโหลดข้อมูลอากาศได้'
        state['offline']=False
        page.wait_for_function("document.getElementById('connection').classList.contains('ok')")
        assert state['posts']==0
        assert not any('/api/rgb' in url for url in requests)
        assert not errors, errors
        browser.close()
finally:
    server.shutdown();server.server_close()
print('PASS: embedded gzip, LDR/RGB removed, weather icons/Thai labels/unknown/stale, desktop/mobile layout, live updates, no RGB requests, location switch, sensor failure/recovery, true zero, pending/401, offline/recovery, no JS errors')
