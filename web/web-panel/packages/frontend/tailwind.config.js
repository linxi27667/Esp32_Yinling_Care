/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{vue,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        primary: {
          DEFAULT: 'var(--md-sys-color-primary)',
          on: 'var(--md-sys-color-on-primary)',
          container: 'var(--md-sys-color-primary-container)',
          'on-container': 'var(--md-sys-color-on-primary-container)',
        },
        secondary: {
          DEFAULT: 'var(--md-sys-color-secondary)',
          on: 'var(--md-sys-color-on-secondary)',
          container: 'var(--md-sys-color-secondary-container)',
          'on-container': 'var(--md-sys-color-on-secondary-container)',
        },
        tertiary: {
          DEFAULT: 'var(--md-sys-color-tertiary)',
          on: 'var(--md-sys-color-on-tertiary)',
          container: 'var(--md-sys-color-tertiary-container)',
          'on-container': 'var(--md-sys-color-on-tertiary-container)',
        },
        error: {
          DEFAULT: 'var(--md-sys-color-error)',
          on: 'var(--md-sys-color-on-error)',
          container: 'var(--md-sys-color-error-container)',
          'on-container': 'var(--md-sys-color-on-error-container)',
        },
        surface: {
          DEFAULT: 'var(--md-sys-color-surface)',
          variant: 'var(--md-sys-color-surface-variant)',
          tint: 'var(--md-sys-color-surface-tint)',
        },
        outline: {
          DEFAULT: 'var(--md-sys-color-outline)',
          variant: 'var(--md-sys-color-outline-variant)',
        },
        background: 'var(--md-sys-color-background)',
        'on-background': 'var(--md-sys-color-on-background)',
        'on-surface': 'var(--md-sys-color-on-surface)',
        'on-surface-variant': 'var(--md-sys-color-on-surface-variant)',
      },
      borderRadius: {
        'md-none': 'var(--md-sys-shape-corner-none)',
        'md-xs': 'var(--md-sys-shape-corner-extra-small)',
        'md-sm': 'var(--md-sys-shape-corner-small)',
        'md-md': 'var(--md-sys-shape-corner-medium)',
        'md-lg': 'var(--md-sys-shape-corner-large)',
        'md-xl': 'var(--md-sys-shape-corner-extra-large)',
        'md-full': 'var(--md-sys-shape-corner-full)',
      },
      boxShadow: {
        'md-1': 'var(--md-sys-elevation-1)',
        'md-2': 'var(--md-sys-elevation-2)',
        'md-3': 'var(--md-sys-elevation-3)',
        'md-4': 'var(--md-sys-elevation-4)',
      },
      transitionTimingFunction: {
        'md-standard': 'var(--md-sys-motion-easing-standard)',
        'md-decelerate': 'var(--md-sys-motion-easing-decelerate)',
        'md-accelerate': 'var(--md-sys-motion-easing-accelerate)',
      },
      transitionDuration: {
        'md-short': 'var(--md-sys-motion-duration-short)',
        'md-medium': 'var(--md-sys-motion-duration-medium)',
        'md-long': 'var(--md-sys-motion-duration-long)',
      },
    },
  },
  plugins: [],
};
