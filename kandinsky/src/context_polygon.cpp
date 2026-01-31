#include <assert.h>
#include <kandinsky/context.h>
#include <poincare/helpers.h>
#include <stdio.h>
#include <stdlib.h>

#include <cmath>

#include "kandinsky/color.h"
#include "kandinsky/coordinate.h"
#include "kandinsky/point.h"

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
        return l[i].x > l[j].x;
      },
      reinterpret_cast<void*>(AEL), n);
}

void KDContext::fillPolygon(KDPoint* vertices, size_t n_vertices, KDColor c) {
  /*Implements the scan-line algorithm
   * https://www.cs.rit.edu/~icss571/filling/how_to.html */

  // First, find the bounding Rect
  KDCoordinate min_x = vertices[0].x();
  KDCoordinate min_y = vertices[0].y();
  KDCoordinate max_x = vertices[0].x();
  KDCoordinate max_y = vertices[0].y();
  for (unsigned int i = 0; i < n_vertices; i++) {
    if (vertices[i].y() < min_y) min_y = vertices[i].y();
    if (vertices[i].y() > max_y) max_y = vertices[i].y();
    if (vertices[i].x() < min_x) min_x = vertices[i].x();
    if (vertices[i].x() > max_x) max_x = vertices[i].x();
  }
  Edge* all_edges = (Edge*)malloc(sizeof(Edge) * n_vertices);
  for (unsigned int i = 0; i < n_vertices; i++) {
    unsigned int j = (i == n_vertices - 1) ? 0 : i + 1;
    unsigned int ii = i;
    if (vertices[i].y() > vertices[j].y()) {
      std::swap(ii, j);
    }
    all_edges[i] = Edge{vertices[ii].y(), vertices[j].y(), vertices[ii].x(),
                        iSlope(vertices[ii], vertices[j])};
  }

  // We sort the list per ymin
  Poincare::Helpers::Sort(
      [](int i, int j, void* context, int n) {
        Edge* edges = reinterpret_cast<Edge*>(context);
        std::swap(edges[i], edges[j]);
      },
      [](int i, int j, void* context, int n) {
        Edge* edges = reinterpret_cast<Edge*>(context);
        return !(
            edges[i].ymin < edges[j].ymin ||
            (edges[i].ymin == edges[j].ymin && edges[i].xmin < edges[j].xmin));
      },
      (void*)all_edges, n_vertices);
  ActiveEdge* AEL = (ActiveEdge*)malloc(sizeof(ActiveEdge) * n_vertices);
  unsigned int an = 0;   // Number of vertices in the AEL
  unsigned int aei = 0;  // Index in the all_edges list
  // Initialise the AEL

  for (KDCoordinate y_intersect = min_y; y_intersect <= max_y; y_intersect++) {
    // Removing edges for which ymax is equal to the scan line, which are thus
    // no longer relevant
    for (int i = an - 1; i >= 0; i--) {
      if (AEL[i].ymax == y_intersect) {
        // We swap with the last in the AEL, if we are not considering the last
        // edge. If we swapped the edge is necessarily one we need to keep,
        // because of the order in which we analyze
        if (i != an - 1) std::swap(AEL[i], AEL[an - 1]);
        an--;
      } else
        AEL[i].x += AEL[i].iSlope;
    }

    // Adding the relevant edges in the AEL
    Edge e = all_edges[aei];
    while (aei < n_vertices && e.ymin == y_intersect) {
      if (!std::isnan(e.iSlope)) {
        AEL[an] = ActiveEdge{(float)e.xmin, e.ymax, e.iSlope};
        an++;
      }
      aei++;
      e = all_edges[aei];
    }

    // Sort the AEL
    sort_AEL(AEL, an);

    for (int i = 0; i < an / 2; i++) {
      KDRect r =
          KDRect(std::ceil(AEL[2 * i].x), y_intersect,
                 std::floor(AEL[2 * i + 1].x) - std::ceil(AEL[2 * i].x), 1);
      KDColor bg;
      KDPoint inf = KDPoint(std::floor(AEL[2 * i].x), y_intersect);
      uint8_t alpha_inf = (uint8_t)((1 - AEL[2 * i].x + (float)inf.x()) * 256);
      getPixel(inf, &bg);
      setPixel(inf, KDColor::Blend(c, bg, alpha_inf));
      KDPoint sup = KDPoint(std::floor(AEL[2 * i + 1].x), y_intersect);
      uint8_t alpha_sup = (uint8_t)((AEL[2 * i + 1].x - (float)sup.x()) * 256);
      getPixel(sup, &bg);
      setPixel(sup, KDColor::Blend(c, bg, alpha_sup));
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
    drawLine(vertices[i], vertices[j], c);
  }
}