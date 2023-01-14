import random

# Replaces the page that was brought earlier to the physical memory (First in, First out)
def FIFO(pages, frame_count):
    frame_list = []
    page_faults = 0
    for page in pages:
        if page not in frame_list:
            # Fill the frame_list to frame_count size
            if len(frame_list) < frame_count:
                frame_list.append(page)
            else:  # Switch to a new page
                frame_list.pop(0)
                frame_list.append(page)

            page_faults += 1

    return page_faults

# Replaces the page that will not be used for the longest time 
def OPT(pages, frame_count):
    frame_list = []
    page_faults = 0
    for i, page in enumerate(pages):
        if page not in frame_list:
            if len(frame_list) < frame_count:
                frame_list.append(page)
            else:
                long_time_unuse = [page, -1]
                no_longer_exist = -1
                for element in frame_list:
                    try:  # Find the max index occurrence of element in pages
                        index_next_element = pages.index(element, i)
                        if index_next_element > long_time_unuse[1]:
                            long_time_unuse = [element, index_next_element]
                    except ValueError:  # The element no longer exist in pages
                        no_longer_exist = element

                if no_longer_exist != -1:
                    frame_list[frame_list.index(no_longer_exist)] = page
                elif long_time_unuse[1] != -1:
                    frame_list[frame_list.index(long_time_unuse[0])] = page
                else:  # Both equal to -1
                    frame_list.pop(0)
                    frame_list.append(page)

            page_faults += 1

    return page_faults

# Replaces the page that was not used the most (Least recently used)
def LRU(pages, frame_count):
    frame_list = []
    page_faults = 0
    for page in pages:
        if page not in frame_list:
            if len(frame_list) < frame_count:
                frame_list.append(page)
            else:
                frame_list.pop(0)
                frame_list.append(page)

            page_faults += 1
        else: # Update the page to end of frame_list
            frame_list.pop(frame_list.index(page))
            frame_list.append(page)
    
    return page_faults

if __name__ == "__main__":
    page_reference_string = [random.randint(0, 9) for _ in range(20)]
    frame_count = 4

    print("Page Reference String:", page_reference_string)
    print("Number of Page Frames:", frame_count)
    
    # Comparison of the 3 algorithms
    print("FIFO page faults:", FIFO(page_reference_string, frame_count))
    print("LRU page faults:", LRU(page_reference_string, frame_count))
    print("OPT page faults:", OPT(page_reference_string, frame_count))
