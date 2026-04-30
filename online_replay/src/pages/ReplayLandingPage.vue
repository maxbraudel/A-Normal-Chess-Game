<script setup>
import ReplayViewer from "../components/ReplayViewer.vue";

const WHITE_KING_TRACK = Object.freeze({
  kind: "piece",
  id: 0
});

const CLOUD_TRACK = Object.freeze({
  kind: "cloud",
  index: 0
});

const FAST_WINDOWED_AUTOPLAY = Object.freeze({
  minTurn: 80,
  maxTurn: 100,
  initialTurn: 80,
  autoplayOnMount: true,
  autoplayIntervalMs: 320,
  loopPlayback: true,
  initialZoom: 1.8,
  toastCooldownMs: 3000
});
</script>

<template>
  <main class="landing-shell">
    <section class="landing-hero">
      <p class="landing-kicker">ANCG Vue Replay</p>
      <h1>Le replay est maintenant un composant Vue embarquable.</h1>
      <p class="landing-lead">
        Le viewer n'est plus traite comme une page a part. Il se monte maintenant dans un conteneur standard,
        avec des props pour afficher ou masquer la barre de lecture, l'overlay du camp au trait, le statut d'echec
        et maintenant un point de vue local qui masque les entites ennemies sous les nuages.
      </p>
    </section>

    <section class="landing-story">
      <div class="landing-copy">
        <p>
          Cet exemple montre une landing page minimale. Le composant garde le canvas, la camera libre, les overlays
          et le rendu actuel, mais il peut maintenant etre place sous n'importe quel bloc de contenu Vue.
        </p>
        <p>
          Dans cet exemple, on conserve les trois couches d'interface du replay. Sur une autre page, tu peux simplement
          passer des props booleennes pour en masquer certaines sans toucher au moteur de rendu.
        </p>
        <p>
          Le second cadre active un point de vue blanc. Quand le brouillard devient occultant plus tard dans la partie,
          les pieces et batiments noirs caches sous les nuages disparaissent comme dans le jeu original.
        </p>
        <p>
          Les viewers de demonstration activent aussi le clic debug sur les cellules. Le troisieme suit en plus
          le roi blanc id 0 et recentre la camera sur lui a chaque changement de frame.
        </p>
        <p>
          Le quatrieme viewer montre le meme systeme sur un nuage. Comme les fronts meteo n'ont pas d'identifiant
          persistant dans le companion, le ciblage se fait logiquement par index de front actif, ici le nuage 0.
        </p>
        <p>
          Le cinquieme viewer montre maintenant les nouveaux parametres du composant: il est borne aux tours 80 a 100,
          demarre tout seul, boucle sur sa fenetre, commence avec un zoom plus serre et tourne a une vitesse de lecture
          plus elevee que les autres.
        </p>
      </div>

      <div class="landing-viewer-grid">
        <article class="landing-viewer-card">
          <header class="landing-frame-header">Vue globale</header>
          <div class="landing-frame">
            <ReplayViewer class="landing-replay" :enable-cell-debug="true" />
          </div>
        </article>

        <article class="landing-viewer-card">
          <header class="landing-frame-header">Point de vue des Blancs</header>
          <div class="landing-frame">
            <ReplayViewer
              class="landing-replay"
              :enable-cell-debug="true"
              :enable-perspective="true"
              perspective-kingdom="white"
            />
          </div>
        </article>

        <article class="landing-viewer-card">
          <header class="landing-frame-header">Tracking du Roi Blanc · id 0</header>
          <div class="landing-frame">
            <ReplayViewer class="landing-replay" :enable-cell-debug="true" :tracked-target="WHITE_KING_TRACK" />
          </div>
        </article>

        <article class="landing-viewer-card">
          <header class="landing-frame-header">Tracking du Nuage 0</header>
          <div class="landing-frame">
            <ReplayViewer
              class="landing-replay"
              :enable-cell-debug="true"
              :tracked-target="CLOUD_TRACK"
              :toast-cooldown-ms="0"
            />
          </div>
        </article>

        <article class="landing-viewer-card">
          <header class="landing-frame-header">Fenetre 80-100 · autoplay rapide</header>
          <div class="landing-frame">
            <ReplayViewer class="landing-replay" :enable-cell-debug="true" v-bind="FAST_WINDOWED_AUTOPLAY" />
          </div>
        </article>
      </div>
    </section>
  </main>
</template>