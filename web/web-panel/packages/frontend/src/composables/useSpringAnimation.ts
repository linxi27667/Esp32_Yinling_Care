export interface SpringConfig {
  stiffness?: number;
  damping?: number;
  mass?: number;
}

export function useSpringAnimation(config: SpringConfig = {}) {
  const { stiffness = 100, damping = 15, mass = 1 } = config;

  function springTransition(duration: number = 400): string {
    return `all ${duration}ms cubic-bezier(0.34, 1.56, 0.64, 1)`;
  }

  function springSoftTransition(duration: number = 500): string {
    return `all ${duration}ms cubic-bezier(0.25, 1, 0.5, 1)`;
  }

  function easeOutTransition(duration: number = 400): string {
    return `all ${duration}ms cubic-bezier(0.16, 1, 0.3, 1)`;
  }

  return {
    stiffness,
    damping,
    mass,
    springTransition,
    springSoftTransition,
    easeOutTransition,
  };
}
