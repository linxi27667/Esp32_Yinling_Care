import { createRouter, createWebHistory } from 'vue-router';
import HomeView from '@/views/HomeView.vue';
import DeviceDetailView from '@/views/DeviceDetailView.vue';

const routes = [
  {
    path: '/',
    name: 'Home',
    component: HomeView,
    meta: { title: '设备总览' },
  },
  {
    path: '/device/:deviceId',
    name: 'DeviceDetail',
    component: DeviceDetailView,
    meta: { title: '设备控制' },
  },
];

const router = createRouter({
  history: createWebHistory(),
  routes,
});

router.beforeEach((to, from, next) => {
  const title = to.meta.title as string;
  if (title) {
    document.title = `银龄智享 - ${title}`;
  }
  next();
});

export default router;
