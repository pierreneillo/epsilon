#include <assert.h>
#include <kandinsky/context.h>
#include <poincare/helpers.h>
#include <stdlib.h>

#include <cmath>

#include "kandinsky/color.h"
#include "kandinsky/coordinate.h"
#include "kandinsky/point.h"
#include "kandinsky/size.h"

struct Edge {
  KDCoordinate ymin;
  KDCoordinate ymax;
  KDCoordinate xmin;  // x associated with the min y value
  float iSlope;
};

struct ActiveEdge {
  float x;
  KDCoordinate ymax;
  float iSlope;
};

float iSlope(KDPoint a, KDPoint b) {
  if (a.x() == b.x()) {
    return 0;
  } else if (a.y() == b.y()) {
    return NAN;
  } else {
    float dx = b.x() - a.x();
    float dy = b.y() - a.y();
    return dx / dy;
  }
}

void sort_AEL(ActiveEdge* AEL, int n) {
  Poincare::Helpers::Sort(
      [](int i, int j, void* context, int n) {
        ActiveEdge* l = reinterpret_cast<ActiveEdge*>(context);
        assert(0 <= i && i < n && 0 <= j && j < n);
        ActiveEdge s = l[i];
        l[i] = l[j];
        l[j] = s;
      },
      [](int i, int j, void* context, int n) {
        ActiveEdge* l = reinterpret_cast<ActiveEdge*>(context);
        assert(0 <= i && i < n && 0 <= j && j < n);
        return l[i].x < l[j].x;
      },
      reinterpret_cast<void*>(AEL), n);
}

void KDContext::fillPolygon(KDPoint* vertices, size_t n_vertices, KDColor c,
                            KDColor background) {
  assert(n_vertices > 2);
  /*Implements the scan-line algorithm
   * https://www.cs.rit.edu/~icss571/filling/how_to.html */

  // First, find the bounding Rect
  KDRect bounding_rect = KDRect(0, 0, 0, 0);
  for (int i = 0; i < n_vertices; i++) {
    if (vertices[i].y() < bounding_rect.top())
      bounding_rect.setOrigin(KDPoint(bounding_rect.left(), vertices[i].y()));
    if (vertices[i].y() > bounding_rect.bottom())
      bounding_rect.setSize(
          KDSize(bounding_rect.width(), vertices[i].y() - bounding_rect.top()));
    if (vertices[i].x() < bounding_rect.left())
      bounding_rect.setOrigin(KDPoint(vertices[i].x(), bounding_rect.top()));
    if (vertices[i].x() > bounding_rect.right())
      bounding_rect.setSize(KDSize(vertices[i].x() - bounding_rect.left(),
                                   bounding_rect.height()));
  }
  Edge* all_edges = (Edge*)malloc(sizeof(Edge) * n_vertices);
  for (size_t i = 0; i < n_vertices; i++) {
    size_t j = (i == n_vertices - 1) ? 0 : i + 1;
    if (vertices[i].y() > vertices[j].y()) {
      std::swap(i, j);
    }
    all_edges[i] = Edge{vertices[i].y(), vertices[j].y(), vertices[i].x(),
                        iSlope(vertices[i], vertices[j])};
  }

  // We sort the list per ymin
  Poincare::Helpers::Sort(
      [](int i, int j, void* context, int n) {
        Edge* edges = reinterpret_cast<Edge*>(context);
        std::swap(edges[i], edges[j]);
      },
      [](int i, int j, void* context, int n) {
        Edge* edges = reinterpret_cast<Edge*>(context);
        return edges[i].ymin < edges[j].ymin ||
               (edges[i].ymin == edges[j].ymin &&
                edges[i].xmin < edges[j].xmin);
      },
      (void*)all_edges, n_vertices);

  ActiveEdge* AEL = (ActiveEdge*)malloc(sizeof(ActiveEdge) * n_vertices);
  int an = 0;   // Number of vertices in the AEL
  int aei = 0;  // Index in the all_edges list

  // Initialise the AEL
  for (KDCoordinate y_intersect = bounding_rect.top();
       y_intersect < bounding_rect.bottom(); y_intersect++) {
    // Removing edges for which ymax is equal to the scan line, which are thus
    // no longer relevant
    for (int i = an - 1; i >= 0; i--) {
      if (AEL[i].ymax == y_intersect) {
        // We swap with the last in the AEL, if we are not considering the last
        // edge. If we swapped the edge is necessarily one we need to keep,
        // because of the order in which we analyze
        if (i != an - 1) std::swap(AEL[i], AEL[an - 1]);
        an--;
      }
    }

    // Adding the relevant edges in the AEL
    Edge e = all_edges[aei];
    while (aei < n_vertices && e.ymin == y_intersect) {
      if (!std::isnan(e.iSlope)) {
        AEL[an] = ActiveEdge{(float)e.xmin, e.ymax, e.iSlope};
        an++;
      }
      e = all_edges[aei];
      aei++;
    }

    // Sort the AEL
    sort_AEL(AEL, an);
    assert(an % 2 == 0);

    for (int i = 0; i < an / 2; i++) {
      KDRect r =
          KDRect(AEL[2 * i].x, y_intersect, AEL[2 * i + 1].x - AEL[2 * i].x, 1);
      fillRect(r, c);
    }
  }
  free(AEL);
  free(all_edges);
}

void KDContext::strokePolygon(KDPoint* vertices, size_t n_vertices, KDColor c,
                              KDColor background) {
  for (size_t i = 0; i < n_vertices; i++) {
    size_t j = (i == n_vertices - 1) ? 0 : i + 1;
    drawAntialiasedLine(vertices[i], vertices[j], c, background);
  }
}