<template>
  <div class="app-container">
    <header class="app-header">
      <div class="header-inner">
        <div class="header-left">
          <div class="logo-mark">
            <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
              <path d="M3 9l9-7 9 7v11a2 2 0 01-2 2H5a2 2 0 01-2-2V9z"/>
              <polyline points="9 22 9 12 15 12 15 22"/>
            </svg>
          </div>
          <span class="logo-text">智慧家居</span>
        </div>
        <div class="header-right">
          <span class="status-badge" :class="connected ? 'online' : 'offline'">
            <span class="status-dot"></span>
            <span class="status-label">{{ connected ? '已连接' : '未连接' }}</span>
          </span>
        </div>
      </div>
    </header>

    <main class="app-main">
      <router-view />
    </main>
  </div>
</template>

<script setup lang="ts">
import { useWebSocket } from '@/composables/useWebSocket';
const { connected } = useWebSocket();
</script>

<style scoped>
.app-container {
  min-height: 100vh;
  background: #f5f5f7;
  color: #1d1d1f;
  font-family: -apple-system, BlinkMacSystemFont, "SF Pro Display", "SF Pro Text", "Helvetica Neue", sans-serif;
}

.app-header {
  position: sticky;
  top: 0;
  z-index: 100;
  background: rgba(255,255,255,0.85);
  backdrop-filter: blur(20px) saturate(180%);
  -webkit-backdrop-filter: blur(20px) saturate(180%);
  border-bottom: 1px solid rgba(0,0,0,0.06);
}

.header-inner {
  max-width: 1200px;
  margin: 0 auto;
  padding: 0 24px;
  height: 52px;
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.header-left {
  display: flex;
  align-items: center;
  gap: 10px;
}

.logo-mark {
  width: 32px;
  height: 32px;
  border-radius: 8px;
  background: linear-gradient(135deg, #0071e3, #40a9ff);
  color: white;
  display: flex;
  align-items: center;
  justify-content: center;
}

.logo-text {
  font-size: 17px;
  font-weight: 600;
  letter-spacing: -0.02em;
}

.header-right {
  display: flex;
  align-items: center;
}

.status-badge {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 4px 12px;
  border-radius: 20px;
  font-size: 12px;
  font-weight: 500;
}

.status-badge.online {
  background: rgba(48, 209, 88, 0.1);
  color: #30d158;
}

.status-badge.offline {
  background: rgba(142, 142, 147, 0.1);
  color: #8e8e93;
}

.status-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
}

.status-badge.online .status-dot {
  background: #30d158;
  box-shadow: 0 0 6px rgba(48, 209, 88, 0.5);
}

.status-badge.offline .status-dot {
  background: #8e8e93;
}

.status-label {
  font-size: 12px;
}

.app-main {
  max-width: 1200px;
  margin: 0 auto;
  padding: 32px 24px;
}
</style>
