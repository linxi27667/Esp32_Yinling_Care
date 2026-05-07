import { ref } from 'vue';

export interface RippleOptions {
  color?: string;
  duration?: number;
}

export function useRipple(options: RippleOptions = {}) {
  const { color = 'rgba(255,255,255,0.3)', duration = 600 } = options;

  function trigger(event: MouseEvent, el: HTMLElement) {
    const rect = el.getBoundingClientRect();
    const size = Math.max(rect.width, rect.height);
    const x = event.clientX - rect.left - size / 2;
    const y = event.clientY - rect.top - size / 2;

    const ripple = document.createElement('span');
    ripple.style.cssText = `
      position: absolute;
      border-radius: 50%;
      background: ${color};
      width: ${size}px;
      height: ${size}px;
      left: ${x}px;
      top: ${y}px;
      pointer-events: none;
      transform: scale(0);
      opacity: 1;
      animation: ripple ${duration}ms ease-out forwards;
    `;

    el.style.position = 'relative';
    el.style.overflow = 'hidden';
    el.appendChild(ripple);

    setTimeout(() => ripple.remove(), duration);
  }

  return { trigger };
}
