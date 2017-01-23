
#include <stdio.h>
#include <libexif/exif-loader.h>

int main(int argc, char **argv)
{
    int rc = 1;
    ExifLoader *l;

    if (argc < 2) {
        printf("Usage: %s image.jpg\n", argv[0]);
        printf("Extracts a thumbnail from the given EXIF image.\n");
        return rc;
    }

    
    l = exif_loader_new();
    if (l) {
        ExifData *ed;

        
        exif_loader_write_file(l, argv[1]);

        
        ed = exif_loader_get_data(l);

	
        exif_loader_unref(l);
	l = NULL;
        if (ed) {
	    
            if (ed->data && ed->size) {
                FILE *thumb;
                char thumb_name[1024];

		
                snprintf(thumb_name, sizeof(thumb_name),
                         "%s_thumb.jpg", argv[1]);

                thumb = fopen(thumb_name, "wb");
                if (thumb) {
		    
                    fwrite(ed->data, 1, ed->size, thumb);
                    fclose(thumb);
                    printf("Wrote thumbnail to %s\n", thumb_name);
                    rc = 0;
                } else {
                    printf("Could not create file %s\n", thumb_name);
                    rc = 2;
                }
            } else {
                printf("No EXIF thumbnail in file %s\n", argv[1]);
                rc = 1;
            }
	    
            exif_data_unref(ed);
        }
    }
    return rc;
}
