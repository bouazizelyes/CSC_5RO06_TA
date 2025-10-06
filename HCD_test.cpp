#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "HCD.h" // Inclut le fichier .h du filtre de correction de distorsion
#include <ap_fixed.h>
typedef ap_fixed<32, 16> harris_fixed_t;
typedef ap_fixed<3, 1> K_fixed_t;

#define IMG_WIDTH 256
#define IMG_HEIGHT 256

int const SIZE = IMG_WIDTH * IMG_HEIGHT;

// Fonction à accélérer en hardware
void HCD_filter_sw(T I_x[IMG_HEIGHT][IMG_WIDTH], T I_y[IMG_HEIGHT][IMG_WIDTH], T output_img[IMG_HEIGHT][IMG_WIDTH]) {

    T S_x2[IMG_HEIGHT][IMG_WIDTH] = {0};
    T S_y2[IMG_HEIGHT][IMG_WIDTH] = {0};
    T S_xy[IMG_HEIGHT][IMG_WIDTH] = {0};

    int ncoins = 0;

    // Calcul pour chaque pixel de l'image entière
    for (int y = 1; y < IMG_HEIGHT - 1; y++) {
        for (int x = 1; x < IMG_WIDTH - 1; x++) {
            // Calcul des produits Ix^2, Iy^2, Ix*Iy pour les fenêtres de lissage
            T Ix2_window[3][3];
            T Iy2_window[3][3];
            T Ixy_window[3][3];

            // Remplir les fenêtres de produits pour le lissage
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int yy = y + ky;
                    int xx = x + kx;
                    Ix2_window[ky + 1][kx + 1] = I_x[yy][xx] * I_x[yy][xx];
                    Iy2_window[ky + 1][kx + 1] = I_y[yy][xx] * I_y[yy][xx];
                    Ixy_window[ky + 1][kx + 1] = I_x[yy][xx] * I_y[yy][xx];
                }
            }

            // Appliquer le noyau de lissage sur les produits calculés
            S_x2[y][x] = apply_kernel_single_block<T>(Ix2_window);
            S_y2[y][x] = apply_kernel_single_block<T>(Iy2_window);
            S_xy[y][x] = apply_kernel_single_block<T>(Ixy_window);

            // Calcul de la réponse Harris
            harris_fixed_t  det_M = (harris_fixed_t )(S_x2[y][x] * S_y2[y][x]) - (harris_fixed_t )(S_xy[y][x] * S_xy[y][x]);
            harris_fixed_t  trace_M = (harris_fixed_t )(S_x2[y][x] + S_y2[y][x]);
            harris_fixed_t  R = det_M - (K_fixed_t)K * (trace_M * trace_M);

            // Détection de coin en appliquant un seuil
            if (R > Rseuil) {
                output_img[y][x] = 1;
                ncoins = ncoins+1;

            } else {
                output_img[y][x] = 0;
            }
        }
    }
    printf("Nombre de coins detectés :%d\n",ncoins);
}

void display_image(T output_img[IMG_HEIGHT][IMG_WIDTH]) {
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            // Affiche '#' pour les pixels détectés comme coins, sinon '.'
        printf("%3d ",output_img[y][x]);

        }
        printf("\n"); // Nouvelle ligne pour chaque ligne de l'image
    }
}


int main(void) {
    T I_x[IMG_HEIGHT][IMG_WIDTH];
    T I_y[IMG_HEIGHT][IMG_WIDTH];
    T output_img_hw[IMG_HEIGHT][IMG_WIDTH];
    T output_img_sw[IMG_HEIGHT][IMG_WIDTH];

    // Initialisation de l'image d'entrée avec des valeurs aléatoires
    srand(0); // Pour un test reproductible
    for (int i = 0; i < IMG_HEIGHT; i++) {
        for (int j = 0; j < IMG_WIDTH; j++) {
            I_x[i][j] = (i + j) % 256; // Gradient horizontal
            I_y[i][j] = (i - j + 256) % 256; // Gradient vertical inversé
        }
    }

    // Applique la fonction hardware pour la détection de coins
    standalone_HCD_filter(I_x, I_y, output_img_hw);

    // Applique la fonction de détection de coins de référence
    HCD_filter_sw(I_x, I_y, output_img_sw);

    // Affiche l'image de sortie pour le résultat software
    //printf("\nImage de sortie (software) :\n");
    //display_image(output_img_sw);

    // Vérification des résultats
    int err = 0;
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            if (output_img_sw[y][x] != output_img_hw[y][x]) {
                if (err < 10) {  // Affiche seulement les 10 premières erreurs
                    printf("Mismatch at [%d][%d]: expected %d, got %d\n", y, x, output_img_sw[y][x], output_img_hw[y][x]);
                }
                err++;
            }
        }
    }

    if (err == 0) {
        printf("Images identical ... Test successful!\n");
    } else {
        printf("Test failed with %d mismatches.\n", err);
    }

    return err;
}
