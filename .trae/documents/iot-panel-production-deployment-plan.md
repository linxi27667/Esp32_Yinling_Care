# IoT 控制面板生产环境部署计划

## 一、兼容性分析

### 1.1 微信小程序兼容性

| 评估维度 | 结果 | 说明 |
|---------|------|------|
| **技术栈兼容性** | ❌ 不兼容 | 基于 Vue 3 + Vite，微信小程序使用 WXML/WXSS，架构完全不同 |
| **DOM 操作** | ❌ 不兼容 | 使用了原生 DOM 操作（Ripple 水波纹效果），小程序无 DOM |
| **WebSocket** | ⚠️ 需改造 | 小程序使用 `wx.connectSocket`，API 不同但概念相似 |
| **外部字体** | ❌ 不兼容 | Google Fonts 和 Material Symbols 在小程序中无法直接加载 |
| **TailwindCSS** | ❌ 不兼容 | 小程序不支持 Tailwind 的类名体系 |
| **结论** | **需要重写** | 必须使用 uni-app / Taro 等跨端框架重新开发 |

**微信小程序改造方案：**
- 使用 **uni-app** (Vue 3) 或 **Taro** 重写
- 替换 WebSocket 为小程序原生 `wx.connectSocket`
- 替换 Google Fonts 为本地字体或小程序安全字体
- 使用小程序原生组件替代自定义 Ripple 动效
- 后端 API 无需改动，保持 WebSocket/MQTT 协议

### 1.2 Android 应用兼容性

| 评估维度 | 结果 | 说明 |
|---------|------|------|
| **WebView 嵌入** | ✅ 兼容 | 可直接用 WebView 加载部署后的 Web 应用 |
| **原生应用** | ⚠️ 需改造 | 当前为 Web 技术栈，非原生 Android |
| **PWA** | ✅ 推荐 | 可添加 PWA 支持，实现类原生体验 |
| **Capacitor / Cordova** | ✅ 推荐 | 可将现有 Vue 项目打包为 Android APK |
| **结论** | **多种方案可选** | 最简单：WebView 嵌入；最佳：Capacitor 打包 |

**Android 部署方案：**
1. **方案 A（推荐）**：使用 Capacitor 将 Vue 项目打包为 Android 应用
2. **方案 B**：部署 Web 应用后，用 WebView 加载
3. **方案 C**：使用 Flutter 重写（工作量最大）

### 1.3 Web 应用部署性

| 评估维度 | 结果 | 说明 |
|---------|------|------|
| **静态资源** | ✅ 可部署 | Vite 构建后生成纯静态文件 |
| **后端依赖** | ⚠️ 需配合 | 需要 Node.js 后端运行 WebSocket + MQTT |
| **环境变量** | ✅ 支持 | 可通过 `.env` 配置不同环境 |
| **构建输出** | ✅ 标准 | `dist/` 目录包含 index.html + 静态资源 |
| **结论** | **完全可部署** | 前后端分离部署，前端可托管到任何静态服务器 |

---

## 二、生产环境部署架构

### 2.1 部署架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              用户访问层                                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │   浏览器     │  │  微信浏览器  │  │ Android App │  │    iOS App (未来)    │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘ │
│         │                │                │                    │            │
│         └────────────────┴────────────────┘                    │            │
│                          │                                     │            │
│                          ▼                                     ▼            │
│                   ┌─────────────┐                    ┌─────────────┐        │
│                   │  CDN / OSS   │                    │  App Store  │        │
│                   │ (静态资源)   │                    │  (APK/IPA)  │        │
│                   └──────┬──────┘                    └─────────────┘        │
└──────────────────────────┼──────────────────────────────────────────────────┘
                           │
                           ▼ HTTPS
┌─────────────────────────────────────────────────────────────────────────────┐
│                             云服务层 (阿里云/腾讯云)                           │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                         Nginx 反向代理                               │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────────┐ │   │
│  │  │  静态文件    │  │  API 路由    │  │      WebSocket 升级          │ │   │
│  │  │  (前端dist)  │  │  /api/*      │  │      /ws → Node.js           │ │   │
│  │  └─────────────┘  └─────────────┘  └─────────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     Node.js 后端服务 (Docker)                        │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────────┐ │   │
│  │  │  Express     │  │  WebSocket   │  │      MQTT 客户端             │ │   │
│  │  │  REST API    │  │  Server      │  │   (连接 broker.emqx.io)     │ │   │
│  │  └─────────────┘  └─────────────┘  └─────────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
└────────────────────────────────────┼────────────────────────────────────────┘
                                     │
                                     ▼ MQTT
┌─────────────────────────────────────────────────────────────────────────────┐
│                           物联网设备层                                        │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │  主机 ESP32  │  │ 一楼 ESP32  │  │ 二楼 ESP32  │  │    三楼 ESP32       │ │
│  │  (MQTT Broker│  │  (从机)      │  │  (从机)      │  │    (从机)           │ │
│  │   客户端)    │  │             │  │             │  │                     │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────────────┘ │
│         │                                                                  │
│         └──────────────────────────► broker.emqx.io:1883 ◄───────────────────┘
│                                           (公共 MQTT Broker)                 │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 技术选型

| 组件 | 技术方案 | 说明 |
|------|---------|------|
| **前端托管** | 阿里云 OSS + CDN | 静态资源托管，全球加速 |
| **反向代理** | Nginx | SSL 终止、负载均衡、静态文件服务 |
| **后端运行** | Docker + PM2 | 容器化部署，进程守护 |
| **云服务** | 阿里云 ECS / 腾讯云 CVM | 2核4G 即可满足 |
| **域名** | 自定义域名 + SSL 证书 | Let's Encrypt 免费证书 |
| **监控** | PM2 + 阿里云监控 | 进程监控、性能监控 |

---

## 三、实施步骤

### Phase 1: 生产环境配置改造

#### Task 1.1: 创建生产环境配置文件

**文件：** `packages/frontend/.env.production`
```
VITE_API_BASE_URL=https://your-domain.com/api
VITE_WS_URL=wss://your-domain.com/ws
```

**文件：** `packages/frontend/.env.development`
```
VITE_API_BASE_URL=http://localhost:3001/api
VITE_WS_URL=ws://localhost:3001/ws
```

#### Task 1.2: 改造前端 WebSocket 连接逻辑

**修改：** `packages/frontend/src/composables/useWebSocket.ts`
- 使用 `import.meta.env.VITE_WS_URL` 替代硬编码 URL
- 添加连接超时处理
- 添加心跳机制（ping/pong）

#### Task 1.3: 改造后端 CORS 和端口配置

**修改：** `packages/backend/src/index.ts`
- 配置允许的生产环境域名
- 支持从环境变量读取端口和 MQTT Broker 地址

### Phase 2: 构建与打包

#### Task 2.1: 前端生产构建

```bash
cd packages/frontend
npm run build
```

**输出：** `packages/frontend/dist/` 目录
- `index.html`
- `assets/` (JS/CSS/图片)

#### Task 2.2: 后端依赖整理

```bash
cd packages/backend
npm install --production
```

### Phase 3: 服务器部署

#### Task 3.1: 服务器环境准备

**操作系统：** Ubuntu 22.04 LTS

**安装依赖：**
```bash
# 更新系统
sudo apt update && sudo apt upgrade -y

# 安装 Node.js 20
 curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install -y nodejs

# 安装 Nginx
sudo apt install -y nginx

# 安装 PM2
sudo npm install -g pm2

# 安装 Docker (可选)
sudo apt install -y docker.io docker-compose
```

#### Task 3.2: Nginx 配置

**文件：** `/etc/nginx/sites-available/xiaozhi-iot`

```nginx
server {
    listen 80;
    server_name your-domain.com;
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name your-domain.com;

    # SSL 证书
    ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;

    # 前端静态文件
    location / {
        root /var/www/xiaozhi-iot/frontend;
        index index.html;
        try_files $uri $uri/ /index.html;
    }

    # API 代理
    location /api/ {
        proxy_pass http://localhost:3001/api/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_cache_bypass $http_upgrade;
    }

    # WebSocket 代理
    location /ws {
        proxy_pass http://localhost:3001/ws;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_read_timeout 86400;
    }
}
```

#### Task 3.3: 部署前端静态文件

```bash
# 创建目录
sudo mkdir -p /var/www/xiaozhi-iot/frontend

# 复制构建产物
sudo cp -r packages/frontend/dist/* /var/www/xiaozhi-iot/frontend/

# 设置权限
sudo chown -R www-data:www-data /var/www/xiaozhi-iot
sudo chmod -R 755 /var/www/xiaozhi-iot
```

#### Task 3.4: 部署后端服务

```bash
# 创建目录
sudo mkdir -p /var/www/xiaozhi-iot/backend

# 复制后端代码
sudo cp -r packages/backend/* /var/www/xiaozhi-iot/backend/

# 安装依赖
cd /var/www/xiaozhi-iot/backend
sudo npm install --production

# 使用 PM2 启动
sudo pm2 start src/index.ts --name xiaozhi-backend --interpreter tsx
sudo pm2 save
sudo pm2 startup
```

### Phase 4: SSL 证书配置

```bash
# 安装 Certbot
sudo apt install -y certbot python3-certbot-nginx

# 申请证书
sudo certbot --nginx -d your-domain.com

# 自动续期
sudo systemctl enable certbot.timer
```

### Phase 5: 安全加固

#### Task 5.1: 防火墙配置

```bash
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow ssh
sudo ufw allow http
sudo ufw allow https
sudo ufw enable
```

#### Task 5.2: 后端安全

- 添加请求速率限制 (Rate Limiting)
- 添加输入验证和 SQL 注入防护
- 配置 JWT 认证（如需用户系统）
- 隐藏服务器版本信息

#### Task 5.3: Nginx 安全头

```nginx
add_header X-Frame-Options "SAMEORIGIN" always;
add_header X-Content-Type-Options "nosniff" always;
add_header X-XSS-Protection "1; mode=block" always;
add_header Referrer-Policy "strict-origin-when-cross-origin" always;
```

### Phase 6: 监控与日志

#### Task 6.1: PM2 监控

```bash
# 查看状态
pm2 status
pm2 logs xiaozhi-backend

# 监控面板
pm2 monit
```

#### Task 6.2: Nginx 日志轮转

```bash
sudo nano /etc/logrotate.d/nginx
```

#### Task 6.3: 健康检查端点

**后端添加：**
```typescript
app.get('/health', (req, res) => {
  res.json({
    status: 'ok',
    timestamp: Date.now(),
    uptime: process.uptime(),
    wsConnections: wss.clients.size,
    mqttConnected: mqttClient.connected
  });
});
```

---

## 四、测试流程

### 4.1 部署前测试

| 测试项 | 命令 | 预期结果 |
|--------|------|---------|
| 前端构建 | `npm run build` | 无错误，dist/ 目录生成 |
| 类型检查 | `npm run type-check` | 无类型错误 |
| 后端启动 | `npm start` | 服务运行在 3001 端口 |
| MQTT 连接 | 查看日志 | 成功连接到 broker.emqx.io |

### 4.2 部署后测试

| 测试项 | 方法 | 预期结果 |
|--------|------|---------|
| HTTP 访问 | `curl https://your-domain.com` | 返回 200，显示页面 |
| WebSocket | 浏览器 DevTools | 连接成功，状态显示"已连接" |
| API 测试 | `curl https://your-domain.com/api/health` | 返回 JSON 状态 |
| SSL 证书 | SSL Labs 测试 | A+ 评级 |
| 性能测试 | Lighthouse | 性能分数 > 90 |

### 4.3 端到端测试

1. **设备上线测试**
   - 启动 ESP32 设备
   - 确认前端显示设备在线
   - 检查心跳更新

2. **控制指令测试**
   - 点击灯光开关
   - 确认 MQTT 命令发出
   - 确认设备响应并更新状态

3. **场景联动测试**
   - 触发"回家"场景
   - 确认多个设备同时响应

4. **并发测试**
   - 多浏览器同时访问
   - 确认 WebSocket 广播正常

---

## 五、Docker 部署方案（可选）

### 5.1 Dockerfile

**文件：** `packages/backend/Dockerfile`

```dockerfile
FROM node:20-alpine

WORKDIR /app

COPY package*.json ./
RUN npm install --production

COPY . .

EXPOSE 3001

CMD ["node", "dist/index.js"]
```

### 5.2 docker-compose.yml

```yaml
version: '3.8'

services:
  backend:
    build: ./packages/backend
    ports:
      - "3001:3001"
    environment:
      - NODE_ENV=production
      - MQTT_BROKER=mqtt://broker.emqx.io:1883
      - PORT=3001
    restart: unless-stopped
    networks:
      - xiaozhi-network

  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf
      - ./packages/frontend/dist:/usr/share/nginx/html
      - ./ssl:/etc/nginx/ssl
    depends_on:
      - backend
    restart: unless-stopped
    networks:
      - xiaozhi-network

networks:
  xiaozhi-network:
    driver: bridge
```

---

## 六、成功标准

| 指标 | 目标值 | 测量方法 |
|------|--------|---------|
| **可用性** | 99.9% | Uptime 监控 |
| **首屏加载** | < 2s | Lighthouse |
| **WebSocket 延迟** | < 100ms | 浏览器 DevTools |
| **并发连接** | > 100 | 压力测试 |
| **SSL 评级** | A+ | SSL Labs |
| **安全漏洞** | 0 高危 | 安全扫描 |

---

## 七、维护与更新流程

### 7.1 日常维护

```bash
# 查看服务状态
pm2 status
sudo systemctl status nginx

# 查看日志
pm2 logs
sudo tail -f /var/log/nginx/access.log

# 更新代码
git pull origin main
npm install
npm run build
pm2 restart xiaozhi-backend
```

### 7.2 备份策略

- **代码：** Git 版本控制
- **配置：** `/etc/nginx/` 和 PM2 配置定期备份
- **日志：** 30 天轮转保留

---

## 八、成本估算（阿里云）

| 资源 | 规格 | 月费用 |
|------|------|--------|
| ECS 服务器 | 2核4G，带宽5M | ~100 元 |
| 域名 | .com 域名 | ~60 元/年 |
| SSL 证书 | Let's Encrypt | 免费 |
| CDN | 按流量计费 | ~20 元 |
| **总计** | | **~120 元/月** |
