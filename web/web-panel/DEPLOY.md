# 银龄智享 IoT 控制面板 - 生产部署指南

## 快速开始

### 方式一：Docker 部署（推荐）

```bash
# 1. 构建前端
cd packages/frontend
npm run build

# 2. 返回根目录，启动 Docker
cd ../..
docker-compose up -d

# 3. 访问应用
open http://localhost
```

### 方式二：手动部署

#### 1. 构建前端

```bash
cd packages/frontend
npm install
npm run build
```

#### 2. 构建后端

```bash
cd packages/backend
npm install --production
```

#### 3. 配置 Nginx

复制 `nginx.conf` 到服务器 `/etc/nginx/conf.d/` 并修改域名。

#### 4. 启动后端

```bash
cd packages/backend
npm start
```

---

## 环境变量配置

### 前端 (.env.production)

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `VITE_API_BASE_URL` | API 基础路径 | `/api` |
| `VITE_WS_URL` | WebSocket 地址 | `wss://your-domain.com/ws` |

### 后端

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `NODE_ENV` | 运行环境 | `development` |
| `PORT` | 服务端口 | `3001` |
| `MQTT_BROKER` | MQTT Broker 地址 | `mqtt://broker.emqx.io:1883` |
| `CORS_ORIGIN` | CORS 允许来源 | `*` |

---

## 测试清单

### 部署前测试

- [ ] 前端构建成功 (`npm run build`)
- [ ] 类型检查通过 (`npm run type-check`)
- [ ] 后端启动成功 (`npm start`)
- [ ] MQTT 连接成功

### 部署后测试

- [ ] HTTP 访问正常 (返回 200)
- [ ] WebSocket 连接成功
- [ ] API 健康检查通过 (`/health`)
- [ ] 设备状态显示正常
- [ ] 控制指令下发成功
- [ ] 场景联动执行正常

---

## 文件说明

| 文件 | 说明 |
|------|------|
| `docker-compose.yml` | Docker 编排配置 |
| `nginx.conf` | Nginx 反向代理配置 |
| `packages/backend/Dockerfile` | 后端 Docker 镜像构建 |
| `packages/frontend/.env.production` | 前端生产环境变量 |
| `packages/frontend/public/manifest.json` | PWA 应用清单 |
| `packages/frontend/public/sw.js` | Service Worker |
