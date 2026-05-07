import { ref, onMounted, onUnmounted } from 'vue';

export function useScrollReveal() {
  const elements = ref<HTMLElement[]>([]);
  let observer: IntersectionObserver | null = null;

  function observe(el: HTMLElement) {
    if (!observer) {
      observer = new IntersectionObserver(
        (entries) => {
          entries.forEach((entry) => {
            if (entry.isIntersecting) {
              entry.target.classList.add('visible');
            }
          });
        },
        { threshold: 0.1, rootMargin: '0px 0px -50px 0px' }
      );
    }
    elements.value.push(el);
    observer.observe(el);
  }

  onUnmounted(() => {
    if (observer) {
      elements.value.forEach((el) => observer!.unobserve(el));
      observer.disconnect();
    }
  });

  return { observe };
}
