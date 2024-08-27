#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

// Estrutura de cabeçalho para arquivos BMP
#pragma pack(push, 1)
typedef struct {
    unsigned short bfType;
    unsigned int bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    unsigned int biSize;
    int biWidth;
    int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    int biXPelsPerMeter;
    int biYPelsPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

// Função para ler uma imagem BMP
int* ler_bmp(const char *nome_arquivo, int *largura, int *altura) {
    FILE *arquivo = fopen(nome_arquivo, "rb");
    if (!arquivo) {
        printf("Erro ao abrir o arquivo.\n");
        return NULL;
    }

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    fread(&fileHeader, sizeof(BITMAPFILEHEADER), 1, arquivo);
    fread(&infoHeader, sizeof(BITMAPINFOHEADER), 1, arquivo);

    *largura = infoHeader.biWidth;
    *altura = abs(infoHeader.biHeight);

    int tamanho_imagem = (*largura) * (*altura) * 3;
    int padding = (4 - ((*largura * 3) % 4)) % 4;

    int *pixels = (int *)malloc((*largura) * (*altura) * sizeof(int));
    if (!pixels) {
        printf("Erro ao alocar memória.\n");
        fclose(arquivo);
        return NULL;
    }

    fseek(arquivo, fileHeader.bfOffBits, SEEK_SET);

    for (int y = 0; y < *altura; y++) {
        for (int x = 0; x < *largura; x++) {
            unsigned char b, g, r;
            fread(&b, 1, 1, arquivo);
            fread(&g, 1, 1, arquivo);
            fread(&r, 1, 1, arquivo);
            pixels[y * (*largura) + x] = (r + g + b) / 3; // Converter para escala de cinza
        }
        fseek(arquivo, padding, SEEK_CUR);
    }

    fclose(arquivo);
    return pixels;
}

// Função para salvar uma imagem BMP
void salvar_bmp(const char *nome_arquivo, int *pixels, int largura, int altura) {
    FILE *arquivo;
    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    int tamanho_imagem = largura * altura * 3;
    int padding = (4 - (largura * 3) % 4) % 4;

    // Configurar cabeçalhos BMP
    fileHeader.bfType = 0x4D42;
    fileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (tamanho_imagem + padding * altura);
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = largura;
    infoHeader.biHeight = -altura; // Altura negativa para origem no canto superior esquerdo
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 24;
    infoHeader.biCompression = 0;
    infoHeader.biSizeImage = tamanho_imagem;
    infoHeader.biXPelsPerMeter = 0;
    infoHeader.biYPelsPerMeter = 0;
    infoHeader.biClrUsed = 0;
    infoHeader.biClrImportant = 0;

    // Abrir arquivo BMP para escrita
    arquivo = fopen(nome_arquivo, "wb");
    if (!arquivo) {
        printf("Erro ao abrir o arquivo para escrita.\n");
        return;
    }

    // Escrever os cabeçalhos no arquivo
    fwrite(&fileHeader, sizeof(BITMAPFILEHEADER), 1, arquivo);
    fwrite(&infoHeader, sizeof(BITMAPINFOHEADER), 1, arquivo);

    // Escrever os pixels da imagem
    for (int y = 0; y < altura; y++) {
        for (int x = 0; x < largura; x++) {
            unsigned char r = (unsigned char)pixels[y * largura + x];
            unsigned char g = r;
            unsigned char b = r;
            fwrite(&b, 1, 1, arquivo);
            fwrite(&g, 1, 1, arquivo);
            fwrite(&r, 1, 1, arquivo);
        }
        // Adicionar padding, se necessário
        for (int p = 0; p < padding; p++) {
            unsigned char padding_byte = 0;
            fwrite(&padding_byte, 1, 1, arquivo);
        }
    }

    fclose(arquivo);
    printf("Imagem salva como %s\n", nome_arquivo);
}

// Função para aplicar o filtro Gaussiano em uma parte da imagem
void aplicar_filtro_gaussiano(int *parte_imagem, int largura, int altura, int kernel_size) {
    // Criar o kernel Gaussiano (aqui um exemplo 3x3)
    double kernel[3][3] = {
        {1.0, 2.0, 1.0},
        {2.0, 4.0, 2.0},
        {1.0, 2.0, 1.0}
    };
    double kernel_soma = 16.0; // Soma dos elementos do kernel

    int *imagem_processada = (int *)malloc(largura * altura * sizeof(int));

    // Percorrer cada pixel da imagem (ignorando as bordas)
    for (int y = 1; y < altura - 1; y++) {
        for (int x = 1; x < largura - 1; x++) {
            double pixel_valor = 0.0;

            // Aplicar o kernel
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int pixel = parte_imagem[(y + ky) * largura + (x + kx)];
                    pixel_valor += pixel * kernel[ky + 1][kx + 1];
                }
            }

            // Normalizar e atribuir ao pixel processado
            imagem_processada[y * largura + x] = (int)(pixel_valor / kernel_soma);
        }
    }

    // Copiar a imagem processada de volta para a parte da imagem original
    for (int y = 1; y < altura - 1; y++) {
        for (int x = 1; x < largura - 1; x++) {
            parte_imagem[y * largura + x] = imagem_processada[y * largura + x];
        }
    }

    free(imagem_processada);
}

int main(int argc, char** argv) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int largura, altura;
    int *imagem_completa = NULL;
    int *parte_imagem = NULL;

    if (rank == 0) {
        // Processo mestre carrega a imagem completa
        imagem_completa = ler_bmp("imagem_original.bmp", &largura, &altura);
        if (!imagem_completa) {
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        int largura_parte = largura / (size - 1); // tamanho da parte da imagem para cada processo

        // Enviar partes da imagem para outros processos
        for (int i = 1; i < size; i++) {
            MPI_Send(imagem_completa + (i - 1) * largura_parte * altura, largura_parte * altura, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    } else {
        // Processo escravo recebe sua parte da imagem
        parte_imagem = (int *)malloc(largura * altura * sizeof(int));
        MPI_Recv(parte_imagem, largura * altura, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Aplicar o filtro Gaussiano
        aplicar_filtro_gaussiano(parte_imagem, largura, altura, 3);

        // Enviar de volta a parte processada ao processo mestre
        MPI_Send(parte_imagem, largura * altura, MPI_INT, 0, 0, MPI_COMM_WORLD);

        free(parte_imagem);
    }

    if (rank == 0) {
        // Receber as partes processadas de volta
        int largura_parte = largura / (size - 1);
        for (int i = 1; i < size; i++) {
            MPI_Recv(imagem_completa + (i - 1) * largura_parte * altura, largura_parte * altura, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Salvar a imagem processada
        salvar_bmp("imagem_processada.bmp", imagem_completa, largura, altura);

        free(imagem_completa);
    }

    MPI_Finalize();
    return 0;
}
